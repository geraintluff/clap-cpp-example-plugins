#pragma once

#include "clap/plugin.h"

#include <vector>

struct RegisteredPlugin {
	const clap_plugin_descriptor *descriptor;
	const clap_plugin *(*create)(const clap_host *host);
};

extern const char * clap_module_resources();
extern std::vector<RegisteredPlugin> registeredPlugins;
