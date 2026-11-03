cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# Static MSVC runtime: the installed .vst3 needs no extra Visual C++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(FLChorus VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: local checkout via -DJUCE_DIR=..., otherwise downloaded automatically
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

set(FLC_FORMATS VST3)
if(APPLE)
    list(APPEND FLC_FORMATS AU)
endif()

option(FLC_COPY_AFTER_BUILD "Copy the plugin into the user's plug-in folder after building" ON)

juce_add_plugin(FLChorus
    COMPANY_NAME "mikita_laurenau"
    PRODUCT_NAME "FL Chorus"
    BUNDLE_ID "com.mikitalaurenau.flchorus"
    PLUGIN_MANUFACTURER_CODE Nikl
    PLUGIN_CODE FChr
    FORMATS ${FLC_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Modulation
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${FLC_COPY_AFTER_BUILD})

juce_add_binary_data(FLCFonts SOURCES
    Resources/fonts/Archivo-ExtraBold.ttf
    Resources/fonts/Archivo-SemiBold.ttf
    Resources/fonts/JetBrainsMono-Bold.ttf)

target_sources(FLChorus PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FLChorus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FLChorus
    PRIVATE
        FLCFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test: runs the processor without a DAW (see tests/)
option(FLC_BUILD_TESTS "Build the processor test" OFF)
if(FLC_BUILD_TESTS)
    juce_add_console_app(processor_test PRODUCT_NAME "processor_test")
    target_sources(processor_test PRIVATE
        tests/processor_test.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(processor_test PRIVATE
        JucePlugin_Name="FL Chorus"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(processor_test PRIVATE FLCFonts juce::juce_audio_utils juce::juce_dsp)
endif()
