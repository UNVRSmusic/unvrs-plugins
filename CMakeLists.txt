cmake_minimum_required(VERSION 3.22)

# Windows: link the MSVC C++ runtime statically, so the plugins do not depend on whatever
# MSVCP140.dll the host has loaded (Ableton ships an older one, which crashes plugins built
# with a newer Visual Studio) and work without the VC++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(S2KConverter VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

add_subdirectory(JUCE)

juce_add_plugin(S2KConverter
    COMPANY_NAME "UNVRS"
    PLUGIN_MANUFACTURER_CODE Diya
    PLUGIN_CODE S2kc
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "S2K Converter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(S2KConverter
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(S2KConverter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(S2KConverter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

add_subdirectory(CrestLimiter)
add_subdirectory(Spool)
add_subdirectory(Nimbus)
add_subdirectory(Ember)
add_subdirectory(Velour)
add_subdirectory(VelourP)
add_subdirectory(Poise)
add_subdirectory(Ictus)
add_subdirectory(Rudiment)
add_subdirectory(Sextant)
add_subdirectory(Pollen)
