cmake_minimum_required(VERSION 3.22)
project(AutoMaster VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AutoMaster
    COMPANY_NAME "Berberian Audio"
    BUNDLE_ID "audio.berberian.automaster"
    PLUGIN_MANUFACTURER_CODE Gbrb
    PLUGIN_CODE Amst
    PRODUCT_NAME "Auto-Master"
    DESCRIPTION "Automatic mastering plugin with BS.1770 LUFS metering, auto-gain, de-harsh dynamic EQ, glue compression, warmth/air EQ, stereo width, and true-peak ceiling limiter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    VST3_CATEGORIES Fx Tools)

# Generate JuceHeader.h for source files that include it.
juce_generate_juce_header(AutoMaster)

target_sources(AutoMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_features(AutoMaster PRIVATE cxx_std_17)

target_compile_definitions(AutoMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AutoMaster PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
