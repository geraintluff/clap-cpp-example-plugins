#include "clap/clap.h"

#include "../plugins.h"
// Helpers for concisely using CLAP from C++
#include "../clap-cpp-tools.h"

#include "signalsmith-basics/chorus.h"

struct ExamplePlugin {
	using Plugin = ExamplePlugin;
	
	static const clap_plugin_descriptor * getPluginDescriptor() {
		static const char * features[] = {
			CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
			CLAP_PLUGIN_FEATURE_STEREO,
			nullptr
		};
		static clap_plugin_descriptor descriptor{
			.id="uk.co.signalsmith-audio.plugins.example-plugin",
			.name="C++ Example Plugin (Chorus)",
			.vendor="Signalsmith Audio",
			.url=nullptr,
			.manual_url=nullptr,
			.support_url=nullptr,
			.version="1.0.0",
			.description="The plugin from a starter CLAP project",
			.features=features
		};
		return &descriptor;
	};

	static const clap_plugin * create(const clap_host *host) {
		return &(new ExamplePlugin(host))->clapPlugin;
	}
	
	const clap_host *host;
	// Extensions aren't filled out until `.pluginInit()`
	const clap_host_state *hostState = nullptr;
	const clap_host_audio_ports *hostAudioPorts = nullptr;
	const clap_host_params *hostParams = nullptr;

	signalsmith::basics::ChorusFloat chorus;

	struct Param {
		double value = 0;
		clap_param_info info;
		const char *formatString = "%.2f";
		
		Param(const char *name, clap_id paramId, double min, double initial, double max) : value(initial) {
			info = {
				.id=paramId,
				.flags=CLAP_PARAM_IS_AUTOMATABLE,
				.cookie=this,
				.name={}, // assigned below
				.module={},
				.min_value=-min,
				.max_value=max,
				.default_value=initial
			};
			std::strncpy(info.name, name, CLAP_NAME_SIZE);
		}
	};
	Param mix{"mix", 0xCA5CADE5, 0, 0.6, 1};
	Param depthMs{"depth", 0xBA55FEED, 2, 15, 50};
	Param detune{"detune", 0xCA55E77E, 1, 6, 30};
	Param stereo{"stereo", 0x0FF51DE5, 0, 1, 2};
	std::array<Param *, 4> params = {&mix, &depthMs, &detune, &stereo};
	
	ExamplePlugin(const clap_host *host) : host(host) {
		depthMs.formatString = "%.1f ms";
		detune.formatString = "%.0f cents";
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
		getHostExtension(host, CLAP_EXT_STATE, hostState);
		getHostExtension(host, CLAP_EXT_AUDIO_PORTS, hostAudioPorts);
		getHostExtension(host, CLAP_EXT_PARAMS, hostParams);
		return true;
	}
	void pluginDestroy() {
		delete this;
	}
	bool pluginActivate(double sRate, uint32_t minFrames, uint32_t maxFrames) {
		chorus.configure(sRate, maxFrames, 2);
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
		chorus.reset();
	}
	void processEvent(const clap_event_header *event) {
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
		if (event->type == CLAP_EVENT_PARAM_VALUE) {
			auto &eventParam = *(const clap_event_param_value *)event;
			if (eventParam.cookie) {
				// if provided, it's the parameter
				auto &param = *(Param *)eventParam.cookie;
				param.value = eventParam.value;
			} else {
				// Otherwise, match the ID
				for (auto *param : params) {
					if (eventParam.param_id == param->info.id) {
						param->value = eventParam.value;
						break;
					}
				}
			}

			// Request a callback so we can tell the host our state is dirty
			stateDirty = true;
			if (hostState) host->request_callback(host);
		}
	}
	clap_process_status pluginProcess(const clap_process *process) {
		auto &audioInput = process->audio_inputs[0];
		auto &audioOutput = process->audio_outputs[0];

		auto *eventsIn = process->in_events;
		auto *eventsOut = process->out_events;
		uint32_t eventCount = eventsIn->size(eventsIn);
		// We could (should?) split the processing up and apply these events partway through the block
		// but for simplicity here we don't support sample-accurate automation
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			processEvent(event);
			eventsOut->try_push(eventsOut, event);
		}
		
		chorus.mix = mix.value;
		chorus.depthMs = depthMs.value;
		chorus.detune = detune.value;
		chorus.stereo = stereo.value;
		chorus.process(audioInput.data32, audioOutput.data32, process->frames_count);
		
		return CLAP_PROCESS_CONTINUE;
	}

	bool stateDirty = false;
	void pluginOnMainThread() {
		if (stateDirty && hostState) {
			hostState->mark_dirty(host);
			stateDirty = false;
		}
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
		}
		return nullptr;
	}
	
	// ---- state save/load ----
	
	bool stateSave(const clap_ostream_t *stream) {
		// very basic string serialisation
		std::string stateString = "hello";
		return writeAllToStream(stateString, stream);
	}
	bool stateLoad(const clap_istream_t *stream) {
		std::string stateString;
		if (!readAllFromStream(stateString, stream) || stateString.empty()) return false;
		return true;
	}

	// ---- audio ports ----

	uint32_t audioPortsCount(bool isInput) {
		return 1;
	}
	bool audioPortsGet(uint32_t index, bool isInput, clap_audio_port_info *info) {
		if (index > audioPortsCount(isInput)) return false;
		*info = {
			.id=0xF0CACC1A,
			.name={'m', 'a', 'i', 'n'},
			.flags=CLAP_AUDIO_PORT_IS_MAIN + CLAP_AUDIO_PORT_REQUIRES_COMMON_SAMPLE_SIZE,
			.channel_count=2,
			.port_type=CLAP_PORT_STEREO,
			.in_place_pair=CLAP_INVALID_ID
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
	}
};
