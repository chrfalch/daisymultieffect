cmake_minimum_required(VERSION 3.22)
project(DaisyMultiFX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find or fetch JUCE
option(FETCH_JUCE "Fetch JUCE from GitHub" ON)

if(FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    # Use local JUCE installation
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "JUCE_DIR must be set when FETCH_JUCE is OFF")
    endif()
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)
endif()

# Core DSP library (shared with firmware)
set(CORE_DIR ${CMAKE_SOURCE_DIR}/../core)

add_library(core_dsp STATIC
    ${CORE_DIR}/audio/audio_processor.cpp
)

target_include_directories(core_dsp PUBLIC
    ${CMAKE_SOURCE_DIR}/..
    ${CORE_DIR}
)

# JUCE Plugin
juce_add_plugin(DaisyMultiFX
    COMPANY_NAME "DaisyMultiFX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE DmFx
    PLUGIN_CODE DmFx
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "DaisyMultiFX"
)

target_sources(DaisyMultiFX PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_include_directories(DaisyMultiFX PRIVATE
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(DaisyMultiFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DaisyMultiFX PRIVATE
    core_dsp
    juce::juce_audio_utils
    juce::juce_audio_processors
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Standalone test (no JUCE dependency)
add_executable(daisy_multifx_test
    src/standalone_test.cpp
)

target_link_libraries(daisy_multifx_test PRIVATE core_dsp)
target_include_directories(daisy_multifx_test PRIVATE
    ${CMAKE_SOURCE_DIR}/..
)
