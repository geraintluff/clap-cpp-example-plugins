################ boilerplate config

cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_CXX_STANDARD 17)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif ()

include(FetchContent)

################ This specific project

project(plugins VERSION 0.1.0)

set(CLAP_NAME_LIST example-synth)
# Each of these should have `sources/${NAME}.cpp`, which defines `clap_init`/`clap_deinit`/`clap_get_factory`
# If your bundle has multiple plugins, you could have extra targets for just developing one at a time

set(CLAP_BUNDLE_PREFIX "uk.co.signalsmith-audio.plugins")
set(CLAP_LINK_LIBRARIES clap)

# Add extra dependencies
add_subdirectory(modules/cbor-walker)
FetchContent_Declare(
	webview-gui
	GIT_REPOSITORY https://github.com/geraintluff/webview-gui.git
	GIT_TAG snapshot-2025-10-16
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(webview-gui)
list(APPEND CLAP_LINK_LIBRARIES webview-gui cbor-walker)

################ CLAP & wrappers

set(CLAP_WRAPPER_OUTPUT_NAME clap-wrapper-target)
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE)
if(NOT DEFINED VST3_SDK_ROOT)
	if(EMSCRIPTEN)
		# don't download the VST3 SDK
		set(VST3_SDK_ROOT "./dummy/vst3sdk/path")
	else()
		set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE)
	endif()
endif()

FetchContent_Declare(
	clap
	GIT_REPOSITORY https://github.com/free-audio/clap.git
	GIT_TAG 1.2.6
	GIT_SHALLOW ON
)
FetchContent_Declare(
	clap-wrapper
	GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
	GIT_TAG v0.13.0
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(clap clap-wrapper)

################ The actual plugin(s)

foreach(NAME ${CLAP_NAME_LIST})
	add_library(${NAME}_static STATIC)
	target_compile_definitions(${NAME}_static PRIVATE
		CLAP_BUNDLE_ID=${TARGET_BUNDLE_PREFIX}.${NAME}
		CLAP_BUNDLE_VERSION=${CMAKE_PROJECT_VERSION}
	)
	target_link_libraries(${NAME}_static PUBLIC
		${CLAP_LINK_LIBRARIES}
	)
	target_sources(${NAME}_static PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/source/${NAME}.cpp
	)

	make_clapfirst_plugins(
		TARGET_NAME ${NAME}
		IMPL_TARGET ${NAME}_static

		RESOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"

		OUTPUT_NAME "${NAME}"
		ENTRY_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/source/clap_entry.cpp

		BUNDLE_IDENTIFIER "${TARGET_BUNDLE_PREFIX}.${NAME}"
		BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
		WINDOWS_FOLDER_VST3 TRUE

		PLUGIN_FORMATS CLAP VST3 WCLAP # AUV2
		COPY_AFTER_BUILD FALSE

		#AUV2_MANUFACTURER_NAME "Signalsmith Audio"
		#AUV2_MANUFACTURER_CODE "SigA"
		#AUV2_SUBTYPE_CODE "BdDt"
		#AUV2_INSTRUMENT_TYPE "aufx"
	)
endforeach()