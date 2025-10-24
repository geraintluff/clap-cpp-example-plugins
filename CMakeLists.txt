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

set(CLAP_NAME cpp-example-plugins)
set(CLAP_BUNDLE_ID "uk.co.signalsmith-audio.plugins.cpp-example-plugins")
set(CLAP_LINK_LIBRARIES clap)

set(CLAP_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/source/plugins.cpp
	${CMAKE_CURRENT_LIST_DIR}/source/example-synth/example-synth.cpp
)

# Add extra dependencies
add_subdirectory(modules/cbor-walker)
add_subdirectory(modules/webview-gui)
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

add_library(${CLAP_NAME}_static STATIC)
target_compile_definitions(${CLAP_NAME}_static PRIVATE
	CLAP_BUNDLE_ID=${CLAP_BUNDLE_ID}
	CLAP_BUNDLE_VERSION=${CMAKE_PROJECT_VERSION}
)
target_link_libraries(${CLAP_NAME}_static PUBLIC
	${CLAP_LINK_LIBRARIES}
)
target_sources(${CLAP_NAME}_static PRIVATE
	${CLAP_SOURCES}
)
make_clapfirst_plugins(
	TARGET_NAME ${CLAP_NAME}
	IMPL_TARGET ${CLAP_NAME}_static

	RESOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/resources"

	OUTPUT_NAME "${CLAP_NAME}"
	ENTRY_SOURCE ${CMAKE_CURRENT_LIST_DIR}/source/clap_entry.cpp

	BUNDLE_IDENTIFIER "${CLAP_BUNDLE_ID}"
	BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
	WINDOWS_FOLDER_VST3 TRUE

	PLUGIN_FORMATS CLAP VST3 WCLAP # AUV2
	COPY_AFTER_BUILD FALSE

	#AUV2_MANUFACTURER_NAME "Signalsmith Audio"
	#AUV2_MANUFACTURER_CODE "SigA"
	#AUV2_SUBTYPE_CODE "BdDt"
	#AUV2_INSTRUMENT_TYPE "aufx"
)
