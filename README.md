# C++ CLAP starter project

⚠️  WIP, only properly tested on MacOS so far

This is a starter CLAP project, aims to show how neat the CLAP API is, 

* Example synth (sine plucks) - no external dependencies
* Example audio plugin (chorus) - uses dependencies in `modules/`
	* `signalsmith-basics` for the chorus itself
	* `cbor-walker` for saving/loading/sending state as CBOR
	* `webview-gui` for

The CMake build uses [`clap-wrapper`](https://github.com/free-audio/clap-wrapper) to (optionally) produce VST3 plugins from the CLAP. 

## Building

CMake:

```sh
mkdir -p out
# Generate (or update) the build project
cmake -B out/build # -G Xcode/whatever
# Build the project
cmake --build out/build --target cpp-example-plugins --config Release
```

If you're not familiar with CMake:

* The `cpp-example-plugins` target will generate `cpp-example-plugins.clap`
* When generating the project, you can specify a particular back-end, e.g. `-G Xcode`.  If you're using the default one, it might not support multiple build configs, so specify `-DCMAKE_BUILD_TYPE=Release` instead
* I personally add `-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=..` when generating as well, which puts the output in `out/` instead of `out/build`

For personal convenience when developing on my Mac, I've included a `Makefile` which calls through to CMake.  It assumes a Mac system with Xcode and REAPER installed, so if you run `make dev-cpp-example-plugins` it will build the plugins and open REAPER to test them.

## Making it your own

* change `CLAP_NAME`/`CLAP_BUNDLE_ID` in `CMakeLists.txt`
* remove extra dependencies from `modules/` (using `git rm`, since they're submodules)
* also remove dependencies from `CMakeLists.txt` (under "This specific project")
* remove unwanted subdirectories from `source/`, and change `CLAP_SOURCES` in `CMakeLists.txt` 

