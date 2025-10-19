#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

#include "./wrap-plugin-method.h"

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
		.init=wrapPluginMethod<&ExampleSynth::pluginInit>(),
		.destroy=wrapPluginMethod<&ExampleSynth::pluginDestroy>(),
		.activate=wrapPluginMethod<&ExampleSynth::pluginActivate>(),
		.deactivate=wrapPluginMethod<&ExampleSynth::pluginDeactivate>(),
		.start_processing=wrapPluginMethod<&ExampleSynth::pluginStartProcessing>(),
		.stop_processing=wrapPluginMethod<&ExampleSynth::pluginStopProcessing>(),
		.reset=wrapPluginMethod<&ExampleSynth::pluginReset>(),
		.process=wrapPluginMethod<&ExampleSynth::pluginProcess>(),
		.get_extension=wrapPluginMethod<&ExampleSynth::pluginGetExtension>(),
		.on_main_thread=wrapPluginMethod<&ExampleSynth::pluginOnMainThread>()
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
		return CLAP_PROCESS_CONTINUE;
	}
	void pluginOnMainThread() {
	}

	const void * pluginGetExtension(const char *extId) {
		if (!std::strcmp(extId, CLAP_EXT_STATE)) {
			static const clap_plugin_state ext{
				.save=wrapPluginMethod<&ExampleSynth::stateSave>(),
				.load=wrapPluginMethod<&ExampleSynth::stateLoad>(),
			};
			return &ext;
		}
		return nullptr;
	}
	
	bool stateSave(const clap_ostream_t *stream) {
		return true;
	}
	bool stateLoad(const clap_istream_t *stream) {
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
