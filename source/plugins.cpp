#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "plugins.h"

#include "clap/clap.h"

#include "./example-plugin/example-plugin.h"
#include "./example-synth/example-synth.h"

#include <cstring>

std::string clapBundleResourceDir;

// ---- Plugin factory ----

static uint32_t pluginFactoryGetPluginCount(const struct clap_plugin_factory *) {
	return 2;
}
static const clap_plugin_descriptor_t * pluginFactoryGetPluginDescriptor(const struct clap_plugin_factory *factory, uint32_t index) {
	if (index == 0) return ExamplePlugin::getPluginDescriptor();
	if (index == 1) return ExampleSynth::getPluginDescriptor();
	return nullptr;
}

static const clap_plugin_t * pluginFactoryCreatePlugin(const struct clap_plugin_factory *, const clap_host_t *host, const char *pluginId) {
	if (!std::strcmp(pluginId, ExamplePlugin::getPluginDescriptor()->id)) {
		return ExamplePlugin::create(host);
	} else if (!std::strcmp(pluginId, ExampleSynth::getPluginDescriptor()->id)) {
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
