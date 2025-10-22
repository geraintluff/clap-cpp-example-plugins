#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

// Helpers for concisely using CLAP from C++
#include "./clap-cpp-tools.h"

#include "./note-manager/note-manager.h"

#include <cstring>
#include <cmath>

static std::string clapBundleResourceDir;

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

	struct Osc {
		float phase = 0;
		float amp = 0;
	};
	std::vector<Osc> oscillators;
	NoteManager noteManager;

	ExampleSynth(const clap_host *host) : host(host) {
		oscillators.resize(noteManager.polyphony());
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
		LOG_EXPR(event->time);
		LOG_EXPR(event->type);
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
		
		noteManager.startBlock();
		auto processNoteTasks = [&]() {
			for (auto &note : noteManager.tasks) {
				auto &osc = oscillators[note.polyIndex];
				
				if (note.state == NoteManager::stateDown) {
					// Start new note
					osc.amp = 0;
				}
				
				float *outputBuffer = synthOut.data32[note.polyIndex%synthOut.channel_count];
				
				auto slewMs = 50 - 49*note.velocity*note.velocity;
				auto ampSlew = 1/(slewMs*0.001f*sampleRate + 1);
				auto targetAmp = (note.released() ? 0 : note.velocity/4);
				auto hz = 440*std::exp2((note.key - 69)/12);
				auto phaseStep = hz/sampleRate;
				
				if (note.state == NoteManager::stateKill) {
					targetAmp = 0;
					float samples = note.processTo - note.processFrom;
					// Decay -60dB in the time we have
					ampSlew = 7/(samples + 7);
				}
				
				for (uint32_t i = note.processFrom; i < note.processTo; ++i) {
					osc.amp += (targetAmp - osc.amp)*ampSlew;
					osc.phase += phaseStep;
					outputBuffer[i] += osc.amp*std::sin(float(2*M_PI)*osc.phase);
				}
				osc.phase -= std::floor(osc.phase);

				if (note.released() && osc.amp < 1e-4f) {
					noteManager.stop(note, process->out_events);
				}
			}
		};

		auto *eventsIn = process->in_events;
		auto *eventsOut = process->out_events;
		uint32_t eventCount = eventsIn->size(eventsIn);
		for (uint32_t i = 0; i < eventCount; ++i) {
			auto *event = eventsIn->get(eventsIn, i);
			if (noteManager.processEvent(event, eventsOut)) {
				processNoteTasks();
			} else {
				processEvent(event);
			}
			eventsOut->try_push(eventsOut, event);
		}
		
		noteManager.processTo(process->frames_count);
		processNoteTasks();
		
		return CLAP_PROCESS_CONTINUE;
	}
	void pluginOnMainThread() {
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
		}
		return nullptr;
	}
	
	bool stateSave(const clap_ostream_t *stream) {
		std::string stateString = "Hello, world!";
		return writeAllToStream(stateString, stream);
	}
	bool stateLoad(const clap_istream_t *stream) {
		std::string stateString;
		if (!readAllFromStream(stateString, stream)) return false;
		return true;
	}
	
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

	uint32_t notePortsCount(bool isInput) {
		return isInput ? 1 : 0;
	}
	bool notePortsGet(uint32_t index, bool isInput, clap_note_port_info *info) {
		if (index > notePortsCount(isInput)) return false;
		*info = {
			.id=0xDEADBA55,
			.supported_dialects=CLAP_NOTE_DIALECT_CLAP,
			.preferred_dialect=CLAP_NOTE_DIALECT_CLAP,
			.name={'n', 'o', 't', 'e', 's'}
		};
		return true;
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
