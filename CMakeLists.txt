cmake_minimum_required(VERSION 3.22)
project(Bridge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Bridge
    VERSION                     "1.0.0"
    COMPANY_NAME                "Bridge Audio"
    BUNDLE_ID                   "com.bridgeaudio.bridge"
    COMPANY_WEBSITE             "https://github.com/mganucheau/bridge"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    BrAu
    PLUGIN_CODE                 Brdg
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Bridge"
    DESCRIPTION                 "Animal Drummer and Bootsy bass in one plugin"
)

juce_generate_juce_header(Bridge)

target_sources(Bridge PRIVATE
    Source/BridgeProcessor.cpp
    Source/BridgeEditor.cpp
    Source/MainPanel.cpp
    Source/AnimalPanel.cpp
    Source/BootsyPanel.cpp
    Source/SteviePanel.cpp
    Source/PaulPanel.cpp
    Source/animal/DrumEngine.cpp
    Source/bootsy/BassEngine.cpp
    Source/stevie/PianoEngine.cpp
    Source/paul/GuitarEngine.cpp
)

target_compile_definitions(Bridge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Bridge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
