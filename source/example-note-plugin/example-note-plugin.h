#include "clap/clap.h"

#include "signalsmith-clap/cpp.h"
#include "signalsmith-clap/note-manager.h"

#include "cbor-walker/cbor-walker.h"
#include "webview-gui/clap-webview-gui.h"

#include "../plugins.h"

#include <atomic>
#include <random>

struct ExampleNotePlugin {
	using Plugin = ExampleNotePlugin;
	
	static const clap_plugin_descriptor * getPluginDescriptor() {
		static const char * features[] = {
			CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
			nullptr
		};
		static clap_plugin_descriptor descriptor{
			.id="uk.co.signalsmith-audio.plugins.example-note-plugin",
			.name="C++ Example Note Plugin",
			.vendor="Signalsmith Audio",
			.url=nullptr,
			.manual_url=nullptr,
			.support_url=nullptr,
			.version="1.0.0",
			.description="Note plugin from a starter CLAP project",
			.features=features
		};
		return &descriptor;
	};

	static const clap_plugin * create(const clap_host *host) {
		return &(new Plugin(host))->clapPlugin;
	}
	
	const clap_host *host;
	// Extensions aren't filled out until `.pluginInit()`
	const clap_host_state *hostState = nullptr;
	const clap_host_audio_ports *hostAudioPorts = nullptr;
	const clap_host_note_ports *hostNotePorts = nullptr;
	const clap_host_params *hostParams = nullptr;

	uint32_t noteIdCounter = 0;
	struct OutputNote {
		int32_t noteId;
	};
	std::vector<OutputNote> outputNotes;
	using NoteManager = signalsmith::clap::NoteManager;
	NoteManager noteManager{512};
	double sampleRate = 1;

	struct Param {
		double value = 0;
		clap_param_info info;
		const char *formatString = "%.2f";
		
		// User interactions which we need to send as events to the host
		std::atomic_flag sentValue = ATOMIC_FLAG_INIT;
		std::atomic_flag sentGestureStart = ATOMIC_FLAG_INIT;
		std::atomic_flag sentGestureEnd =ATOMIC_FLAG_INIT;

		std::atomic_flag sentUiState = ATOMIC_FLAG_INIT;

		Param(const char *name, clap_id paramId, double min, double initial, double max) : value(initial) {
			info = {
				.id=paramId,
				.flags=CLAP_PARAM_IS_AUTOMATABLE,
				.cookie=this,
				.name={}, // assigned below
				.module={},
				.min_value=min,
				.max_value=max,
				.default_value=initial
			};
			std::strncpy(info.name, name, CLAP_NAME_SIZE);
			
			sentValue.test_and_set();
			sentGestureStart.test_and_set();
			sentGestureEnd.test_and_set();
		}

		void sendEvents(const clap_output_events *outEvents) {
			if (!sentGestureStart.test_and_set()) {
				clap_event_param_gesture event{
					.header={
						.size=sizeof(clap_event_param_gesture),
						.time=0,
						.space_id=CLAP_CORE_EVENT_SPACE_ID,
						.type=CLAP_EVENT_PARAM_GESTURE_BEGIN,
						.flags=CLAP_EVENT_IS_LIVE
					},
					.param_id=info.id
				};
				outEvents->try_push(outEvents, &event.header);
			}
			if (!sentValue.test_and_set()) {
				clap_event_param_value event{
					.header={
						.size=sizeof(clap_event_param_value),
						.time=0,
						.space_id=CLAP_CORE_EVENT_SPACE_ID,
						.type=CLAP_EVENT_PARAM_VALUE,
						.flags=CLAP_EVENT_IS_LIVE
					},
					.param_id=info.id,
					.cookie=this,
					.note_id=-1,
					.port_index=-1,
					.channel=-1,
					.key=-1,
					.value=value
				};
				outEvents->try_push(outEvents, &event.header);
			}
			if (!sentGestureEnd.test_and_set()) {
				clap_event_param_gesture event{
					.header={
						.size=sizeof(clap_event_param_gesture),
						.time=0,
						.space_id=CLAP_CORE_EVENT_SPACE_ID,
						.type=CLAP_EVENT_PARAM_GESTURE_END,
						.flags=CLAP_EVENT_IS_LIVE
					},
					.param_id=info.id
				};
				outEvents->try_push(outEvents, &event.header);
			}
		}
	};
	std::array<Param *, 0> params;
	
	ExampleNotePlugin(const clap_host *host) : host(host) {
		outputNotes.resize(noteManager.polyphony());
	}

	// Makes a C function pointer to a C++ method
	template<auto methodPtr>
	auto clapPluginMethod() -> decltype(signalsmith::clap::pluginMethod<methodPtr>()) {
		return signalsmith::clap::pluginMethod<methodPtr>();
	}

	const clap_plugin clapPlugin{
		.desc=getPluginDescriptor(),
		.plugin_data=this,
		.init=clapPluginMethod<&Plugin::pluginInit>(),
		.destroy=clapPluginMethod<&Plugin::pluginDestroy>(),
		.activate=clapPluginMethod<&Plugin::pluginActivate>(),
		.deactivate=clapPluginMethod<&Plugin::pluginDeactivate>(),
		.start_processing=clapPluginMethod<&Plugin::pluginStartProcessing>(),
		.stop_processing=clapPluginMethod<&Plugin::pluginStopProcessing>(),
		.reset=clapPluginMethod<&Plugin::pluginReset>(),
		.process=clapPluginMethod<&Plugin::pluginProcess>(),
		.get_extension=clapPluginMethod<&Plugin::pluginGetExtension>(),
		.on_main_thread=clapPluginMethod<&Plugin::pluginOnMainThread>()
	};

	bool pluginInit() {
		using namespace signalsmith::clap;
		getHostExtension(host, CLAP_EXT_STATE, hostState);
		getHostExtension(host, CLAP_EXT_AUDIO_PORTS, hostAudioPorts);
		getHostExtension(host, CLAP_EXT_NOTE_PORTS, hostNotePorts);
		getHostExtension(host, CLAP_EXT_PARAMS, hostParams);
		webview.init(&clapPlugin, host, clapBundleResourceDir);
		return true;
	}
	void pluginDestroy() {
		delete this;
	}
	bool pluginActivate(double sRate, uint32_t minFrames, uint32_t maxFrames) {
		sampleRate = sRate;
		return true;
	}
	void pluginDeactivate() {
	}
	bool pluginStartProcessing() {
		return true;
	}
	void pluginStopProcessing() {
	}
	void pluginReset() {
		noteManager.reset();
	}
	void processEvent(const clap_event_header *event) {
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
		if (event->type == CLAP_EVENT_PARAM_VALUE) {
			auto &eventParam = *(const clap_event_param_value *)event;
			if (eventParam.cookie) {
				// if provided, it's the parameter
				auto &param = *(Param *)eventParam.cookie;
				param.value = eventParam.value;
				param.sentUiState.clear();
			} else {
				// Otherwise, match the ID
				for (auto *param : params) {
					if (eventParam.param_id == param->info.id) {
						param->value = eventParam.value;
						param->sentUiState.clear();
						break;
					}
				}
			}

			// Request a callback so we can tell the host our state is dirty
			stateDirty = true;
			// Tell the UI as well
			sentWebviewState.clear();
			host->request_callback(host);
		}
	}
	std::uniform_real_distribution<double> unitReal{0, 1};
	clap_process_status pluginProcess(const clap_process *process) {
		auto *eventsOut = process->out_events;

		noteManager.startBlock();
		auto processNoteTasks = [&](auto &tasks) {
			for (auto &note : tasks) {
				auto &outNote = outputNotes[note.voiceIndex];
				
				if (note.state == NoteManager::stateDown || note.state == NoteManager::stateLegato) {
					outNote.noteId = int32_t(noteIdCounter++);
					if (noteIdCounter >= 0x80000000) noteIdCounter = 0;
					double vIn = note.velocity, vRand = unitReal(randomEngine);
					double velocity = vIn*vRand/(1 - vIn - vRand + 2*vIn*vRand);
					clap_event_note event{
						.header={
							.size=sizeof(clap_event_note),
							.time=note.processFrom,
							.space_id=CLAP_CORE_EVENT_SPACE_ID,
							.type=CLAP_EVENT_NOTE_ON,
							.flags=0
						},
						.note_id=outNote.noteId,
						.port_index=note.port,
						.channel=note.channel,
						.key=note.baseKey,
						.velocity=velocity
					};
					eventsOut->try_push(eventsOut, &event.header);
				} else if (note.state == NoteManager::stateUp) {
					noteManager.stop(note, process->out_events);
					clap_event_note event{
						.header={
							.size=sizeof(clap_event_note),
							.time=note.processFrom,
							.space_id=CLAP_CORE_EVENT_SPACE_ID,
							.type=CLAP_EVENT_NOTE_OFF,
							.flags=0
						},
						.note_id=outNote.noteId,
						.port_index=note.port,
						.channel=note.channel,
						.key=note.baseKey,
						.velocity=note.velocity
					};
					eventsOut->try_push(eventsOut, &event.header);
				}
			}
		};

		auto *eventsIn = process->in_events;
		uint32_t eventCount = eventsIn->size(eventsIn);
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			if (auto newNote = noteManager.wouldStart(event)) {
				bool foundLegato = false;
				processNoteTasks(noteManager.start(*newNote, eventsOut));
			} else if (auto endNote = noteManager.wouldRelease(event)) {
				processNoteTasks(noteManager.release(*endNote));
			}
			processEvent(event);
			eventsOut->try_push(eventsOut, event);
		}
		
		processNoteTasks(noteManager.processTo(process->frames_count));
		return CLAP_PROCESS_CONTINUE;
	}

	bool stateDirty = false;
	void pluginOnMainThread() {
		if (stateDirty && hostState) {
			hostState->mark_dirty(host);
			stateDirty = false;
		}
		webviewSendIfNeeded();
	}

	const void * pluginGetExtension(const char *extId) {
		if (!std::strcmp(extId, CLAP_EXT_STATE)) {
			static const clap_plugin_state ext{
				.save=clapPluginMethod<&Plugin::stateSave>(),
				.load=clapPluginMethod<&Plugin::stateLoad>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_AUDIO_PORTS)) {
			static const clap_plugin_audio_ports ext{
				.count=clapPluginMethod<&Plugin::audioPortsCount>(),
				.get=clapPluginMethod<&Plugin::audioPortsGet>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_NOTE_PORTS)) {
			static const clap_plugin_note_ports ext{
				.count=clapPluginMethod<&Plugin::notePortsCount>(),
				.get=clapPluginMethod<&Plugin::notePortsGet>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_PARAMS)) {
			static const clap_plugin_params ext{
				.count=clapPluginMethod<&Plugin::paramsCount>(),
				.get_info=clapPluginMethod<&Plugin::paramsGetInfo>(),
				.get_value=clapPluginMethod<&Plugin::paramsGetValue>(),
				.value_to_text=clapPluginMethod<&Plugin::paramsValueToText>(),
				.text_to_value=clapPluginMethod<&Plugin::paramsTextToValue>(),
				.flush=clapPluginMethod<&Plugin::paramsFlush>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, webview_gui::CLAP_EXT_WEBVIEW)) {
			static const webview_gui::clap_plugin_webview ext{
				.get_uri=clapPluginMethod<&Plugin::webviewGetUri>(),
				.get_resource=clapPluginMethod<&Plugin::webviewGetResource>(),
				.receive=clapPluginMethod<&Plugin::webviewReceive>(),
			};
			return &ext;
		}
		return webview.getExtension(extId);
	}
	
	// ---- state save/load ----
	
	bool stateSave(const clap_ostream_t *stream) {
		std::vector<unsigned char> bytes;
		signalsmith::cbor::CborWriter cbor{bytes};
		cbor.openMap(4);
		for (auto *param : params) {
			cbor.addInt(param->info.id); // CBOR keys can be any type
			cbor.addFloat(param->value);
		}
		return signalsmith::clap::writeAllToStream(bytes, stream);
	}
	bool stateLoad(const clap_istream_t *stream) {
		std::vector<unsigned char> bytes;
		if (!signalsmith::clap::readAllFromStream(bytes, stream) || bytes.empty()) return false;

		using Cbor = signalsmith::cbor::CborWalker;
		Cbor cbor{bytes};
		if (!cbor.isMap()) return false;
		cbor.forEachPair([&](Cbor key, Cbor value){
			for (auto *param : params) {
				if (uint32_t(key) == param->info.id) {
					param->value = double(value);
				}
			}
		});
		return true;
	}

	// ---- audio ports ----

	// some hosts (*cough* REAPER *cough*) give us a stereo input/output port unless we support this extension to say we have none
	uint32_t audioPortsCount(bool isInput) {
		return 0;
	}
	bool audioPortsGet(uint32_t index, bool isInput, clap_audio_port_info *info) {
		return false;
	}

	// ---- note ports ----

	uint32_t notePortsCount(bool isInput) {
		return 1;
	}
	bool notePortsGet(uint32_t index, bool isInput, clap_note_port_info *info) {
		if (index > notePortsCount(isInput)) return false;
		*info = {
			.id=0xC0DEBA55,
			.supported_dialects=CLAP_NOTE_DIALECT_CLAP,
			.preferred_dialect=CLAP_NOTE_DIALECT_CLAP,
			.name={'n', 'o', 't', 'e', 's'}
		};
		return true;
	}

	// ---- parameters ----
	
	uint32_t paramsCount() {
		return uint32_t(params.size());
	}
	
	bool paramsGetInfo(uint32_t index, clap_param_info *info) {
		if (index >= params.size()) return false;
		*info = params[index]->info;
		return true;
	}
	
	bool paramsGetValue(clap_id paramId, double *value) {
		for (auto *param : params) {
			if (param->info.id == paramId) {
				*value = param->value;
				return true;
			}
		}
		return false;
	}
	
	bool paramsValueToText(clap_id paramId, double value, char *text, uint32_t textCapacity) {
		for (auto *param : params) {
			if (param->info.id == paramId) {
				std::snprintf(text, textCapacity, param->formatString, value);
				return true;
			}
		}
		return false;
	}

	bool paramsTextToValue(clap_id paramId, const char *text, double *value) {
		return false; // not supported
	}
	
	void paramsFlush(const clap_input_events *eventsIn, const clap_output_events *eventsOut) {
		uint32_t eventCount = eventsIn->size(eventsIn);
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			processEvent(event);
			eventsOut->try_push(eventsOut, event);
		}
		for (auto *param : params) {
			param->sendEvents(eventsOut);
		}
	}

	// ---- GUI ----
	
	static void * pluginToWebview(const clap_plugin *plugin) {
		return &((Plugin *)plugin->plugin_data)->webview;
	}
	webview_gui::ClapWebviewGui<pluginToWebview> webview;
	std::atomic_flag sentWebviewState = ATOMIC_FLAG_INIT;
	
	int32_t webviewGetUri(char *uri, uint32_t uri_capacity) {
		if (uri) std::strncpy(uri, "/", uri_capacity);
		return 1;
	}
	
	bool webviewGetResource(const char *path, char *mediaType, uint32_t mediaTypeCapacity, const clap_ostream *stream) {
		strncpy(mediaType, "text/html;charset=utf-8", mediaTypeCapacity);
		std::string html = "Random number: " + std::to_string(unitReal(randomEngine));
		return signalsmith::clap::writeAllToStream(html, stream);
	}

	bool webviewReceive(const void *bytes, uint32_t length) {
		using Cbor = signalsmith::cbor::CborWalker;
		
		auto updateParam = [&](Param &param, Cbor cbor){
			cbor.forEachPair([&](Cbor key, Cbor value){
				auto keyString = key.utf8View();
				if (keyString == "value" && value.isNumber()) {
					param.value = value;
					param.sentValue.clear();
				} else if (keyString == "gesture") {
					if (bool(value)) {
						param.sentGestureStart.clear();
					} else {
						param.sentGestureEnd.clear();
					}
				}
			});
		};
		
		Cbor cbor{(const unsigned char *)bytes, length};
		if (cbor.utf8View() == "ready") {
			for (auto *param : params) {
				// Resend everything
				param->sentUiState.clear();
			}
			sentWebviewState.clear();
			webviewSendIfNeeded();
			return true;
		}
		
		cbor.forEachPair([&](Cbor key, Cbor value){
			auto keyString = key.utf8View();
//			if (keyString == "mix") {
//				updateParam(mix, value);
//			} else if (keyString == "depth") {
//				updateParam(depthMs, value);
//			} else if (keyString == "detune") {
//				updateParam(detune, value);
//			} else if (keyString == "stereo") {
//				updateParam(stereo, value);
//			}
		});

		if (hostParams) hostParams->request_flush(host);

		return !cbor.error();
	}
	void webviewSendIfNeeded() {
		if (sentWebviewState.test_and_set()) return;

		std::vector<unsigned char> bytes;
		signalsmith::cbor::CborWriter cbor{bytes};
		cbor.openMap();
		
		auto updateParam = [&](const char *key, Param &param){
			if (param.sentUiState.test_and_set()) return;
			cbor.addUtf8(key);
			cbor.openMap(1);
			cbor.addUtf8("value");
			cbor.addFloat(param.value);
		};
//		updateParam("mix", mix);
//		updateParam("depth", depthMs);
//		updateParam("detune", detune);
//		updateParam("stereo", stereo);
		cbor.close();
		
		webview.send(bytes.data(), bytes.size());
	}
	
	std::default_random_engine randomEngine{std::random_device{}()};
};
