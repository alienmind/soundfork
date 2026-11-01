cmake_minimum_required(VERSION 3.22)

project(SoundFork VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# CI turns this off: a runner has no plugin folders worth installing into.
option(SOUNDFORK_COPY_AFTER_BUILD "Install plugins to the system after building" ON)

juce_add_plugin(SoundFork
    COMPANY_NAME "AlienMind"
    PLUGIN_MANUFACTURER_CODE Jlpz
    PLUGIN_CODE Sfrk
    FORMATS VST3 AAX Standalone
    PRODUCT_NAME "SoundFork"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SOUNDFORK_COPY_AFTER_BUILD}
    # The default Avid folder needs elevation and AAX is untested, so it stays in the build tree.
    AAX_COPY_DIR "${CMAKE_BINARY_DIR}/aax-install"
    VST3_CATEGORIES "Fx"
    AAX_CATEGORY AAX_ePlugInCategory_Effect
)

target_sources(SoundFork PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SecondaryOutputDevice.cpp
    Source/DeviceProfile.cpp
)

# images/disabled.png, stopped.png and playing.png. The editor draws a placeholder
# for any that are missing.
file(GLOB SOUNDFORK_IMAGES CONFIGURE_DEPENDS images/*.png)
if(SOUNDFORK_IMAGES)
    juce_add_binary_data(SoundForkImages SOURCES ${SOUNDFORK_IMAGES})
    # Without matching LTO flags, MSVC fails to resolve BinaryData::getNamedResource (LNK2001).
    target_link_libraries(SoundForkImages PRIVATE juce::juce_recommended_lto_flags)
    target_link_libraries(SoundFork PRIVATE SoundForkImages)
    target_compile_definitions(SoundFork PRIVATE SOUNDFORK_HAS_IMAGES=1)
endif()

target_compile_definitions(SoundFork PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SoundFork
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
