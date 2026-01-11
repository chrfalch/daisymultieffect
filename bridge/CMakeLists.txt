cmake_minimum_required(VERSION 3.22)
project(DaisyMidiBridge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Create the GUI application
juce_add_gui_app(DaisyMidiBridge
    COMPANY_NAME "DaisyMultiFX"
    PRODUCT_NAME "Daisy MIDI Bridge"
    BUNDLE_ID "com.daisymultifx.midibridge"
    ICON_BIG ""
    ICON_SMALL ""
)

target_sources(DaisyMidiBridge PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    src/MidiBridge.cpp
)

target_compile_definitions(DaisyMidiBridge PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DaisyMidiBridge,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DaisyMidiBridge,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DaisyMidiBridge PRIVATE
    juce::juce_audio_devices
    juce::juce_gui_basics
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
