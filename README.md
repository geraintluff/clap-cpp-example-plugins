# Signalsmith's CLAP Base

<p style="text-align:center">⚠️ WIP, only properly tested on MacOS so far</p>

> The CLAP API is neat and elegant, and can be used from C++ in a very bare-bones way, and over time, I've started to prefer using this directly instead of using a framework to abstract things.
> 
> However, there are still some helpers and common dependencies which I use a lot, so I've started packaging them up.  They are independent, so you can pick-and-choose, and none of them are top-level classes you *must* inherit from.
>
> -- Geraint (Signalsmith)

## Example plugins

There are also some example plugins, which aim to show both how the CLAP API can be used from C++, and how some of the helpers work.

* Example synth (sine plucks) - no dependencies aside from this repo (and `clap-wrappers` to make VST3/CLAP/... from a common target)
* Example audio plugin (chorus) - uses dependencies in `modules/`
	* `signalsmith-basics` for the chorus itself
	* `cbor-walker` for saving/loading/sending state as CBOR
	* `webview-gui` (used directly)
* Example note plugin (velocity randomiser) - dependency from `modules/`:
	* `webview-gui` (CLAP webview helper)

The CMake build uses [`clap-wrapper`](https://github.com/free-audio/clap-wrapper) to (optionally) produce VST3 plugins from the CLAP. 

### Building

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
