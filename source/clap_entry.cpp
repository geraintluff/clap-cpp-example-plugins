#include "clap/entry.h"
/*
#include "../../stfx/clap/stfx-clap.h"

stfx::clap::Plugins stfxPlugins;
extern void addAnalyser();
extern void addCrunch();
extern void addLimiter();
extern void addReverb();

bool clap_init(const char *path) {
	static bool added = false;
	if (!added) {
		addAnalyser();
		addCrunch();
		addLimiter();
		addReverb();
	}
	return added = stfxPlugins.clap_init(path);
}
void clap_deinit() {
	stfxPlugins.clap_deinit();
}
const void * clap_get_factory(const char *id) {
	return stfxPlugins.clap_get_factory(id);
}
*/

extern bool clap_init(const char *path);
extern void clap_deinit();
extern const void * clap_get_factory(const char *id);

extern "C" {
	const CLAP_EXPORT clap_plugin_entry clap_entry{
		.clap_version = CLAP_VERSION,
		.init = clap_init,
		.deinit = clap_deinit,
		.get_factory = clap_get_factory
	};
}
