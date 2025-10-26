#include "example-plugin.h"

namespace example_plugin_resources {
#include "./resources/index.html.hxx"
#include "./resources/cbor.min.js.hxx"
};

bool ExamplePlugin::webviewGetResource(const char *path, WebviewGui::Resource &resource) {
	if (!std::strcmp(path, "/")) {
		resource.mediaType = "text/html";
		resource.bytes.assign(
			(const unsigned char *)example_plugin_resources::index_html,
			(const unsigned char *)example_plugin_resources::index_html + sizeof(example_plugin_resources::index_html)
		);
		return true;
	} else if (!std::strcmp(path, "/cbor.min.js")) {
		resource.mediaType = "application/javascript";
		resource.bytes.assign(
			(const unsigned char *)example_plugin_resources::cbor_min_js,
			(const unsigned char *)example_plugin_resources::cbor_min_js + sizeof(example_plugin_resources::cbor_min_js)
		);
		return true;
	}
	return false;
}
