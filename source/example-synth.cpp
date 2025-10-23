#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

// Helpers for concisely using CLAP from C++
#include "./clap-cpp-tools.h"

#include "./synth-manager/synth-manager.h"

#include <cstring>
#include <cmath>
#include <cstdio>

static std::string clapBundleResourceDir;

struct Osc {
	float phase = 0;
	float normFreq = 0;
	float attackRelease = 0;
	float decay = 1;
	
	bool canStop() const {
		return attackRelease < 1e-4f;
	}
};

struct ExampleSynth {
	static const clap_plugin_descriptor * getPluginDescriptor() {
		static const char * features[] = {
			CLAP_PLUGIN_FEATURE_INSTRUMENT,
			CLAP_PLUGIN_FEATURE_STEREO,
			nullptr
		};
		static clap_plugin_descriptor descriptor{
			.id="uk.co.signalsmith-audio.plugins.example-synth",
			.name="Example Synth",
			.vendor="Signalsmith Audio",
			.url=nullptr,
			.manual_url=nullptr,
			.support_url=nullptr,
			.version="1.0.0",
			.description="The synth from a starter CLAP project",
			.features=features
		};
		return &descriptor;
	};

	static const clap_plugin * create(const clap_host *host) {
		return &(new ExampleSynth(host))->clapPlugin;
	}
	
	const clap_host *host;
	// Extensions aren't filled out until `.pluginInit()`
	const clap_host_state *hostState = nullptr;
	const clap_host_audio_ports *hostAudioPorts = nullptr;
	const clap_host_note_ports *hostNotePorts = nullptr;
	const clap_host_params *hostParams = nullptr;

	std::vector<Osc> oscillators;
	SynthManager synthManager;
	
	struct {
		clap_id id = 0xCA55E77E;
		double value = -20;
	} sustainDb;
	struct {
		clap_id id = 0xCA5CADE5;
		int value = 1;
	} polyphony;

	ExampleSynth(const clap_host *host) : host(host) {
		oscillators.resize(synthManager.polyphony());
	}
	
	const clap_plugin clapPlugin{
		.desc=getPluginDescriptor(),
		.plugin_data=this,
		.init=clapPluginMethod<&ExampleSynth::pluginInit>(),
		.destroy=clapPluginMethod<&ExampleSynth::pluginDestroy>(),
		.activate=clapPluginMethod<&ExampleSynth::pluginActivate>(),
		.deactivate=clapPluginMethod<&ExampleSynth::pluginDeactivate>(),
		.start_processing=clapPluginMethod<&ExampleSynth::pluginStartProcessing>(),
		.stop_processing=clapPluginMethod<&ExampleSynth::pluginStopProcessing>(),
		.reset=clapPluginMethod<&ExampleSynth::pluginReset>(),
		.process=clapPluginMethod<&ExampleSynth::pluginProcess>(),
		.get_extension=clapPluginMethod<&ExampleSynth::pluginGetExtension>(),
		.on_main_thread=clapPluginMethod<&ExampleSynth::pluginOnMainThread>()
	};

	bool pluginInit() {
		getHostExtension(host, CLAP_EXT_STATE, hostState);
		getHostExtension(host, CLAP_EXT_AUDIO_PORTS, hostAudioPorts);
		getHostExtension(host, CLAP_EXT_NOTE_PORTS, hostNotePorts);
		getHostExtension(host, CLAP_EXT_PARAMS, hostParams);
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
		synthManager.reset();
	}
	void processEvent(const clap_event_header *event) {
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
		if (event->type == CLAP_EVENT_PARAM_VALUE) {
			auto &eventParam = *(const clap_event_param_value *)event;
			if (eventParam.param_id == sustainDb.id) {
				sustainDb.value = eventParam.value;
			} else if (eventParam.param_id == polyphony.id) {
				polyphony.value = int(std::round(eventParam.value));
			}

			// Request a callback so we can tell the host our state is dirty
			stateDirty = true;
			if (hostState) host->request_callback(host);
		}
	}
	clap_process_status pluginProcess(const clap_process *process) {
		for (uint32_t outPort = 0; outPort < process->audio_outputs_count; ++outPort) {
			auto &outBuffer = process->audio_outputs[outPort];
			if (outPort < process->audio_inputs_count) {
				auto &inBuffer = process->audio_inputs[outPort];
				// Copy input
				for (uint32_t outC = 0; outC < outBuffer.channel_count; ++outC) {
					uint32_t inC = outC%(inBuffer.channel_count);
					if (outBuffer.data32 && inBuffer.data32) {
						memcpy(outBuffer.data32[outC], inBuffer.data32[inC], process->frames_count*4);
					}
				}
			} else {
				// Zero
				for (uint32_t outC = 0; outC < outBuffer.channel_count; ++outC) {
					if (outBuffer.data32) {
						memset(outBuffer.data32[outC], 0, process->frames_count*4);
					}
				}
			}
		}

		auto &synthOut = process->audio_outputs[0];
		
		synthManager.startBlock();
		auto processNoteTasks = [&](auto &tasks) {
			float sustainAmp = std::pow(10, sustainDb.value/20);
			
			for (auto &note : tasks) {
				auto &osc = oscillators[note.voiceIndex];

				auto hz = 440*std::exp2((note.key - 69)/12);
				auto targetNormFreq = hz/sampleRate;

				auto portamentoMs = 10;
				auto portamentoSlew = 1/(portamentoMs*0.001f*sampleRate + 1);

				if (note.state == SynthManager::stateDown) {
					// Start new note
					osc = {};
					osc.normFreq = targetNormFreq;
				} else if (note.state == SynthManager::stateLegato) {
					// Restart attack from wherever the decay got to
					osc.attackRelease *= osc.decay;
					osc.decay = 1;
				}
				
				auto arMs = (note.released() ? 50 : 2);
				auto arSlew = 1/(arMs*0.001f*sampleRate + 1);
				auto targetAr = (note.released() ? 0 : note.velocity/4);
				// decay rate
				auto decayMs = 10 + 490*note.velocity*note.velocity;
				auto decaySlew = 1/(decayMs*0.001f*sampleRate + 1);
				
				auto processTo = note.processTo;
				if (note.state == SynthManager::stateKill) { // This note is about to be stolen
					// minimum 1ms fade-out
					processTo = std::max<uint32_t>(note.processTo, note.processFrom + sampleRate*0.001);
					// unless we'd hit the end of the block
					processTo = std::min(processTo, process->frames_count);
					
					// Decay -60dB in the time we have
					float samples = processTo - note.processFrom;
					arSlew = 7/(samples + 7);
					targetAr = 0;
				}
				
				for (uint32_t i = note.processFrom; i < processTo; ++i) {
					osc.attackRelease += (targetAr - osc.attackRelease)*arSlew;
					osc.decay += (sustainAmp - osc.decay)*decaySlew;
					osc.normFreq += (targetNormFreq - osc.normFreq)*portamentoSlew;
					osc.phase += osc.normFreq;
					auto amp = osc.attackRelease*osc.decay;
					auto v = amp*std::sin(float(2*M_PI)*osc.phase);
					// stereo out
					synthOut.data32[0][i] += v;
					synthOut.data32[1][i] += v;
				}
				osc.phase -= std::floor(osc.phase);

				if (note.released() && osc.canStop()) {
					synthManager.stop(note, process->out_events);
				}
			}
		};

		auto *eventsIn = process->in_events;
		auto *eventsOut = process->out_events;
		uint32_t eventCount = eventsIn->size(eventsIn);
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			auto newNote = synthManager.wouldStart(event);
			if (newNote) {
				bool foundLegato = false;
				if (polyphony.value == 0) {
					for (auto &otherNote : synthManager) {
						if (!otherNote.released() || otherNote.ageAt(event->time) < sampleRate*0.01f) {
							processNoteTasks(synthManager.legato(*newNote, otherNote, eventsOut));
							foundLegato = true;
							break;
						}
					}
				}
				if (!foundLegato) {
					processNoteTasks(synthManager.start(*newNote, eventsOut));
				}
			} else {
				processNoteTasks(synthManager.processEvent(event, eventsOut));
			}
			processEvent(event);
			eventsOut->try_push(eventsOut, event);
		}
		
		processNoteTasks(synthManager.processTo(process->frames_count));
		
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
				.save=clapPluginMethod<&ExampleSynth::stateSave>(),
				.load=clapPluginMethod<&ExampleSynth::stateLoad>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_AUDIO_PORTS)) {
			static const clap_plugin_audio_ports ext{
				.count=clapPluginMethod<&ExampleSynth::audioPortsCount>(),
				.get=clapPluginMethod<&ExampleSynth::audioPortsGet>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_NOTE_PORTS)) {
			static const clap_plugin_note_ports ext{
				.count=clapPluginMethod<&ExampleSynth::notePortsCount>(),
				.get=clapPluginMethod<&ExampleSynth::notePortsGet>(),
			};
			return &ext;
		} else if (!std::strcmp(extId, CLAP_EXT_PARAMS)) {
			static const clap_plugin_params ext{
				.count=clapPluginMethod<&ExampleSynth::paramsCount>(),
				.get_info=clapPluginMethod<&ExampleSynth::paramsGetInfo>(),
				.get_value=clapPluginMethod<&ExampleSynth::paramsGetValue>(),
				.value_to_text=clapPluginMethod<&ExampleSynth::paramsValueToText>(),
				.text_to_value=clapPluginMethod<&ExampleSynth::paramsTextToValue>(),
				.flush=clapPluginMethod<&ExampleSynth::paramsFlush>(),
			};
			return &ext;
		}
		return nullptr;
	}
	
	// ---- state save/load ----
	
	bool stateSave(const clap_ostream_t *stream) {
		// very basic string serialisation
		std::string stateString = (polyphony.value ? "P" : "M") + std::to_string(sustainDb.value);
		return writeAllToStream(stateString, stream);
	}
	bool stateLoad(const clap_istream_t *stream) {
		std::string stateString;
		if (!readAllFromStream(stateString, stream) || stateString.empty()) return false;
		polyphony.value = (stateString[0] == 'P' ? 1 : 0);
		auto value = strtod(stateString.c_str() + 1, nullptr);
		if (value >= -40 && value <= 0) {
			sustainDb.value = value;
			return true;
		}
		return false;
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

	// ---- note ports ----

	uint32_t notePortsCount(bool isInput) {
		return isInput ? 1 : 0;
	}
	bool notePortsGet(uint32_t index, bool isInput, clap_note_port_info *info) {
		if (index > notePortsCount(isInput)) return false; // input only
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
		return 2;
	}
	
	bool paramsGetInfo(uint32_t index, clap_param_info *info) {
		if (index == 0) {
			*info = {
				.id=sustainDb.id,
				.flags=CLAP_PARAM_IS_AUTOMATABLE,
				.cookie=nullptr,
				.name={}, // assigned below
				.module={},
				.min_value=-40,
				.max_value=0,
				.default_value=-20
			};
			std::strncpy(info->name, "sustain", CLAP_NAME_SIZE);
			return true;
		} else if (index == 1) {
			*info = {
				.id=polyphony.id,
				.flags=CLAP_PARAM_IS_AUTOMATABLE + CLAP_PARAM_IS_STEPPED,
				.cookie=nullptr,
				.name={}, // assigned below
				.module={},
				.min_value=0,
				.max_value=1,
				.default_value=1
			};
			std::strncpy(info->name, "polyphony", CLAP_NAME_SIZE);
			return true;
		}
		return false;
	}
	
	bool paramsGetValue(clap_id paramId, double *value) {
		if (paramId == sustainDb.id) {
			*value = sustainDb.value;
			return true;
		} else if (paramId == polyphony.id) {
			*value = double(polyphony.value);
		}
		return false;
	}
	
	bool paramsValueToText(clap_id paramId, double value, char *text, uint32_t textCapacity) {
		if (paramId == sustainDb.id) {
			std::snprintf(text, textCapacity, "%i dB", int(std::round(value)));
			return true;
		} else if (paramId == polyphony.id) {
			std::strncpy(text, std::round(value) == 0 ? "monophonic" : "polyphonic", textCapacity);
			return true;
		}
		return false;
	}

	bool paramsTextToValue(clap_id paramId, const char *text, double *value) {
		if (paramId == sustainDb.id) {
			char *numberEnd;
			*value = std::strtod(text, &numberEnd);
			if (!(*value >= 50)) *value = 50;
			if (!(*value <= 500)) *value = 500;
			return (numberEnd != text);
		}
		return false;
	}
	
	void paramsFlush(const clap_input_events *eventsIn, const clap_output_events *eventsOut) {
		uint32_t eventCount = eventsIn->size(eventsIn);
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			processEvent(event);
			eventsOut->try_push(eventsOut, event);
		}
	}
private:
	float sampleRate = 1;
};

// ---- Plugin factory ----

static uint32_t pluginFactoryGetPluginCount(const struct clap_plugin_factory *) {
	return 1;
}
static const clap_plugin_descriptor_t * pluginFactoryGetPluginDescriptor(const struct clap_plugin_factory *factory, uint32_t index) {
	if (index == 0) return ExampleSynth::getPluginDescriptor();
	return nullptr;
}

static const clap_plugin_t * pluginFactoryCreatePlugin(const struct clap_plugin_factory *, const clap_host_t *host, const char *pluginId) {
	if (!std::strcmp(pluginId, ExampleSynth::getPluginDescriptor()->id)) {
		return ExampleSynth::create(host);
	}
	return nullptr;
}

// ---- Main bundle methods ----

bool clapEntryInit(const char *path) {
	clapBundleResourceDir = path;
#if defined(__APPLE__) && __APPLE__ && defined(TARGET_OS_OSX) && TARGET_OS_OSX
	clapBundleResourceDir += "/Contents/Resources";
#endif
	return true;
}
void clapEntryDeinit() {
	clapBundleResourceDir = "";
}

const void * clapEntryGetFactory(const char *factoryId) {
	if (!std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID)) {
		static const clap_plugin_factory clapPluginFactory{
			.get_plugin_count=pluginFactoryGetPluginCount,
			.get_plugin_descriptor=pluginFactoryGetPluginDescriptor,
			.create_plugin=pluginFactoryCreatePlugin
		};
		return &clapPluginFactory;
	}
}
