#include "clap/entry.h"

extern bool clapEntryInit(const char *path);
extern void clapEntryDeinit();
extern const void * clapEntryGetFactory(const char *factoryId);

extern "C" {
	const CLAP_EXPORT clap_plugin_entry clap_entry{
		.clap_version=CLAP_VERSION,
		.init=clapEntryInit,
		.deinit=clapEntryDeinit,
		.get_factory=clapEntryGetFactory
	};
}
