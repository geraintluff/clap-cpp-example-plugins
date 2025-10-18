#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

struct ExampleSynth {
	static const clap_plugin_descriptor * pluginDescriptor() {
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
		return new clap_plugin({
			.desc=pluginDescriptor(),
			.plugin_data=new ExampleSynth(host),
			.init=pluginInit,
			.destroy=pluginDestroy,
			.activate=pluginActivate,
			.deactivate=pluginDeactivate,
			.start_processing=pluginStartProcessing,
			.stop_processing=pluginStopProcessing,
			.reset=pluginReset,
			.process=pluginProcess,
			.get_extension=pluginGetExtension,
			.on_main_thread=pluginOnMainThread
		});
	}
	
	const clap_host *host;

	ExampleSynth(const clap_host *host) : host(host) {
		
	}
	
	static bool pluginInit(const clap_plugin *plugin) {
		auto &self = *(ExampleSynth *)plugin->plugin_data;
		return true;
	}
	static void pluginDestroy(const clap_plugin *plugin) {
		delete (ExampleSynth *)plugin->plugin_data;
		delete plugin;
	}
	static bool pluginActivate(const clap_plugin *plugin, double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
		return true;
	}
	static void pluginDeactivate(const clap_plugin *plugin) {
	}
	static bool pluginStartProcessing(const clap_plugin *plugin) {
		return true;
	}
	static void pluginStopProcessing(const clap_plugin *plugin) {
	}
	static void pluginReset(const clap_plugin *plugin) {
	}
	static clap_process_status pluginProcess(const clap_plugin *plugin, const clap_process *process) {
		return CLAP_PROCESS_CONTINUE;
	}
	static const void * pluginGetExtension(const clap_plugin *plugin, const char *extId) {
		return nullptr;
	}
	static void pluginOnMainThread(const clap_plugin *plugin) {
	}
};

#include "./plugin-list.h"

std::vector<RegisteredPlugin> registeredPlugins = {
	{
		ExampleSynth::pluginDescriptor(),
		ExampleSynth::create
	}
};
