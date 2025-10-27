#include "example-audio-plugin.h"

namespace example_audio_plugin_resources {
#include "./resources/index.html.hxx"
#include "./resources/cbor.min.js.hxx"
};

bool ExampleAudioPlugin::webviewGetResource(const char *path, WebviewGui::Resource &resource) {
	using namespace example_audio_plugin_resources;
	if (!std::strcmp(path, "/")) {
		resource.mediaType = "text/html";
		resource.bytes.assign(
			(const unsigned char *)index_html,
			(const unsigned char *)index_html + sizeof(index_html)
		);
		return true;
	} else if (!std::strcmp(path, "/cbor.min.js")) {
		resource.mediaType = "application/javascript";
		resource.bytes.assign(
			(const unsigned char *)cbor_min_js,
			(const unsigned char *)cbor_min_js + sizeof(cbor_min_js)
		);
		return true;
	}
	return false;
}
