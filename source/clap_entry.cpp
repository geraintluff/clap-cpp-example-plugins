#include "clap/clap.h"

#include "./plugin-list.h"

static std::string clapBundleResourceDir;
const char * clap_module_resources() {
	return clapBundleResourceDir.c_str();
}

#include <cstring>

// ---- Plugin factory ----

static uint32_t pluginFactoryGetPluginCount(const struct clap_plugin_factory *) {
	return uint32_t(registeredPlugins.size());
}
static const clap_plugin_descriptor_t * pluginFactoryGetPluginDescriptor(const struct clap_plugin_factory *, uint32_t index) {
	if (index >= registeredPlugins.size()) return nullptr;
	return registeredPlugins[index].descriptor;
}

static const clap_plugin_t * pluginFactoryCreatePlugin(const struct clap_plugin_factory *, const clap_host_t *host, const char *pluginId) {
	for (auto &entry : registeredPlugins) {
		if (!std::strcmp(pluginId, entry.descriptor->id)) {
			return entry.create(host);
		}
	}
	return nullptr;
}

// ---- Main bundle methods ----

static bool clap_init(const char *path) {
	clapBundleResourceDir = path;
#if __APPLE__ && TARGET_OS_MAC
	clapBundleResourceDir += "/Contents/Resources";
#endif
	return true;
}
static void clap_deinit() {
	clapBundleResourceDir = "";
}

static const void * clap_get_factory(const char *factoryId) {
	if (!std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID)) {
		static const clap_plugin_factory clapPluginFactory{
			.get_plugin_count=pluginFactoryGetPluginCount,
			.get_plugin_descriptor=pluginFactoryGetPluginDescriptor,
			.create_plugin=pluginFactoryCreatePlugin
		};
		return &clapPluginFactory;
	}
}

// ---- CLAP module export ----

extern "C" {
	const CLAP_EXPORT clap_plugin_entry clap_entry{
		.clap_version = CLAP_VERSION,
		.init = clap_init,
		.deinit = clap_deinit,
		.get_factory = clap_get_factory
	};
}
