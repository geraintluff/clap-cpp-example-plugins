# C++ CLAP starter project

This is a starter CLAP project, mostly for internal use, but perhaps helpful for others too. 🙂

It aims to show how neat the CLAP API is, so it uses it directly (no `clap-helpers`).

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

To remove the default plugin(s) and start making your own, change `CLAP_NAME_LIST`/`CLAP_BUNDLE_PREFIX` in `CMakeLists.txt`, and rename/copy the `source/???.cpp` files to match.

You can also remove extra dependencies (e.g. the webview stuff) just below that in the `CMakeLists.txt`.

You can also remove any subdirectories you're not using from `source/`.
