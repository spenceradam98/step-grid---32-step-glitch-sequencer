cmake_minimum_required(VERSION 3.22)

# macOS 11 (Big Sur) on Intel is the minimum; build universal (x86_64 + arm64) on CI.
# No -march flags anywhere: the DSP is scalar C++ and runs on any x86-64 (no AVX2 needed).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redist needed
endif()
project(StepGrid VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core engine (no JUCE dependency, unit-testable) ----
add_library(stepgrid_engine STATIC
    Source/engine/Engine.cpp
    Source/engine/Pattern.cpp
    Source/engine/Presets.cpp)
target_include_directories(stepgrid_engine PUBLIC Source/engine)
set_target_properties(stepgrid_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(STEPGRID_BUILD_TESTS "Build engine tests" ON)
if(STEPGRID_BUILD_TESTS)
    add_executable(stepgrid_tests tests/EngineTests.cpp)
    target_link_libraries(stepgrid_tests PRIVATE stepgrid_engine)
    enable_testing()
    add_test(NAME engine COMMAND stepgrid_tests)
endif()

option(STEPGRID_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT STEPGRID_BUILD_PLUGIN)
    return()
endif()

# ---- JUCE ----
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(STEPGRID_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND STEPGRID_FORMATS AU)
endif()

juce_add_plugin(StepGrid
    COMPANY_NAME "StepGridAudio"
    PLUGIN_MANUFACTURER_CODE Stgd
    PLUGIN_CODE Sgr1
    FORMATS ${STEPGRID_FORMATS}
    PRODUCT_NAME "Step Grid"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Filter
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

set(STEPGRID_PLUGIN_SOURCES
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/Themes.cpp)
set(STEPGRID_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(StepGrid PRIVATE ${STEPGRID_PLUGIN_SOURCES})
target_compile_definitions(StepGrid PUBLIC ${STEPGRID_DEFS})
target_link_libraries(StepGrid
    PRIVATE
        stepgrid_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Screenshot renderer (developer tool, off by default) ----
option(STEPGRID_BUILD_SNAPSHOT "Build the docs screenshot renderer" OFF)
if(STEPGRID_BUILD_SNAPSHOT)
    juce_add_console_app(StepGridSnapshot PRODUCT_NAME "StepGridSnapshot")
    target_sources(StepGridSnapshot PRIVATE tools/Snapshot.cpp ${STEPGRID_PLUGIN_SOURCES})
    target_compile_definitions(StepGridSnapshot PRIVATE ${STEPGRID_DEFS} JucePlugin_Name="Step Grid")
    target_link_libraries(StepGridSnapshot PRIVATE stepgrid_engine juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)
endif()
