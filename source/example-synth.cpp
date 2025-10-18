#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "clap/clap.h"

static std::string resourceDir;

bool clap_init(const char *path) {
	resourceDir = path;
	LOG_EXPR(resourceDir);
	return true;
//	plugins.add<stfx_sfz::SFZ_STFX>({
//		.clap_version = CLAP_VERSION,
//		.id = "uk.co.signalsmith.dev.stfx-sfz",
//		.name = "STFX - SFZ test",
//		.vendor = "Signalsmith Audio",
//		.url = "",
//		.manual_url = "",
//		.support_url = "",
//		.version = "1.0.0"
//	}, {
//		CLAP_PLUGIN_FEATURE_INSTRUMENT,
//		CLAP_PLUGIN_FEATURE_SAMPLER,
//		CLAP_PLUGIN_FEATURE_STEREO
//	});
//
//	return plugins.clap_init(path);
}
void clap_deinit() {
//	plugins.clap_deinit();
}
const void * clap_get_factory(const char *id) {
	return nullptr;
//	return plugins.clap_get_factory(id);
}
