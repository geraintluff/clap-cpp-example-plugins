#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "./example-note-plugin.h"

#include "signalsmith-clap/cpp.h"

#include <cstring>

namespace example_note_plugin_resources {
#include "./resources/index.html.hxx"
#include "./resources/cbor.min.js.hxx"
};

bool ExampleNotePlugin::webviewGetResource(const char *path, char *mediaType, uint32_t mediaTypeCapacity, const clap_ostream *stream) {
	using namespace example_note_plugin_resources;

	const char *type = nullptr;
	std::vector<unsigned char> bytes;

	if (!std::strcmp(path, "/example-note-plugin/")) {
		type = "text/html";
		bytes.assign(
			(const unsigned char *)index_html,
			(const unsigned char *)index_html + sizeof(index_html)
		);
	} else if (!std::strcmp(path, "/example-note-plugin/cbor.min.js")) {
		type = "application/javascript";
		bytes.assign(
			(const unsigned char *)cbor_min_js,
			(const unsigned char *)cbor_min_js + sizeof(cbor_min_js)
		);
	}
	
	if (!type) return false;
	std::strncpy(mediaType, type, mediaTypeCapacity);
	return signalsmith::clap::writeAllToStream(bytes, stream);
}
