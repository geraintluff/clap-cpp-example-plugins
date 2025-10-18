.PHONY: emsdk
all:
	@echo "make clap-bundlenamegoeshere\nmake vst3-???\nmake dev-???\n\nWCLAP with Emscripten:\nmake wclap-"

clean:
	rm -rf out

out/build: CMakeLists.txt
	cmake . -B out/build $(CMAKE_PARAMS) -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=.. -G Xcode

clap-%: out/build
	cmake --build out/build --target $*_clap --config Release

vst3-%: out/build
	cmake --build out/build --target $*_vst3 --config Release

####### Open a test project in REAPER #######

%.RPP:
	mkdir -p `dirname "$@"`
	echo "<REAPER_PROJECT 0.1\n>" > "$@"

reaper-rescan-clap:
	rm -f ~/Library/Application\ Support/REAPER/reaper-clap-*.ini

dev-%: clap-%
	$(MAKE) out/REAPER/$*.RPP
	# Symlink the plugin
	pushd ~/Library/Audio/Plug-Ins/CLAP \
		&& rm -f "$*.clap" \
		&& ln -s "$(CURRENT_DIR)/out/Release/$*.clap"
	# Symlink the bundle's Resources directory
	pushd out/Release/$*.clap/Contents/; rm -rf Resources; ln -s "$(CURRENT_DIR)/resources" Resources
	/Applications/REAPER.app/Contents/MacOS/REAPER out/REAPER/$*.RPP

####### Emscripten #######
# This automatically installs the Emscripten SDK
# if the environment variable EMSDK is not already set

CURRENT_DIR := $(shell pwd)
EMSDK ?= $(CURRENT_DIR)/emsdk
EMSDK_ENV = unset CMAKE_TOOLCHAIN_FILE; EMSDK_QUIET=1 . "$(EMSDK)/emsdk_env.sh";

emsdk:
	@ if ! test -d "$(EMSDK)" ;\
	then \
		echo "SDK not found - cloning from Github" ;\
		git clone https://github.com/emscripten-core/emsdk.git "$(EMSDK)" ;\
		cd "$(EMSDK)" && git pull && ./emsdk install latest && ./emsdk activate latest ;\
		$(EMSDK_ENV) emcc --check && python3 --version && cmake --version ;\
	fi

out/build-emscripten: emsdk
	$(EMSDK_ENV) emcmake cmake . -B out/build-emscripten -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=../Release -DCMAKE_BUILD_TYPE=Release

wclap-%: out/build-emscripten
	$(EMSDK_ENV) cmake --build out/build-emscripten --target $* --config Release
