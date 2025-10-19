#pragma once

#include "clap/plugin.h"
#include "clap/stream.h"

template <typename T>
struct ClapPluginMethodHelper;

// Returns a plain-C function which calls a given C++ method
template<auto methodPtr>
auto clapPluginMethod() {
	using C = ClapPluginMethodHelper<decltype(methodPtr)>;
	return C::template callMethod<methodPtr>;
}

// Partial specialisation used to expand the method signature
template <class Object, typename Return, typename... Args>
struct ClapPluginMethodHelper<Return (Object::*)(Args...)> {
	// Templated static method which forwards to a specific method
	template<Return (Object::*methodPtr)(Args...)>
	static Return callMethod(const clap_plugin *plugin, Args... args) {
		auto *obj = (Object *)plugin->plugin_data;
		return (obj->*methodPtr)(args...);
	}
};

// ---- read/write `std::vector` using CLAP stream(s) ----

template<class Container>
bool writeAllToStream(const Container &c, const clap_ostream *ostream) {
	return writeAllToStream((const void *)c.data(), c.size()*sizeof(c[0]), ostream);
}

bool writeAllToStream(const void *buffer, size_t length, const clap_ostream *ostream) {
	size_t index = 0;
	while (length > index) {
		int64_t result = ostream->write(ostream, (const void *)((size_t)buffer + index), uint64_t(length - index));
		if (result <= 0) return false;
		index += result;
	}
	return true;
}

template<class Container>
bool readAllFromStream(Container &byteContainer, const clap_istream *istream, size_t chunkBytes=1024) {
	while (1) {
		size_t index = byteContainer.size();
		vector.resize(index + chunkBytes);
		int64_t result = istream->read(istream, (void *)((size_t)container.data() + bytes), uint64_t(chunkBytes));
		if (result == chunkBytes) {
			continue;
		} else if (result >= 0) {
			bytes += result;
			vector.resize(index + result);
			if (result == 0) return true;
		} else {
			return false;
		}
	}
}
