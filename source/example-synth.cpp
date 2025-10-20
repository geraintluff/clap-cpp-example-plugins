#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

#include "./clap-extras.h"
#include "./clap-cbor.h"

#include <cstring>

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

	ExampleSynth(const clap_host *host) : host(host) {
		
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
		// This is a normal C++ method
		return true;
	}
	void pluginDestroy() {
		delete this;
	}
	bool pluginActivate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
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
};

#include "./plugin-list.h"

std::vector<RegisteredPlugin> registeredPlugins = {
	{
		ExampleSynth::getPluginDescriptor(),
		ExampleSynth::create
	}
};
