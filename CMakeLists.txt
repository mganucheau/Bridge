cmake_minimum_required(VERSION 3.22)
project(Bridge VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    DESCRIPTION                 "Drums, bass, keys, and guitar MIDI ensemble in one plugin"
)

juce_generate_juce_header(Bridge)

# So #include "LeaderStylePresets.h" and other flat Source/ headers resolve for all toolchains/IDEs.
target_include_directories(Bridge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_sources(Bridge PRIVATE
    Source/BridgeProcessor.cpp
    Source/BridgeEditor.cpp
    Source/BridgeHeaderBar.cpp
    Source/BridgeEditorChromeLookAndFeel.cpp
    Source/BridgeInstrumentTabTile.cpp
    Source/BridgeInstrumentTabRail.cpp
    Source/BridgeBottomHalf.cpp
    Source/BridgeLoopRangeStrip.cpp
    Source/BridgeXYPad.cpp
    Source/BridgeSettingsDialog.cpp
    Source/BridgeLookAndFeel.cpp
    Source/BridgeTheme.cpp
    Source/InstrumentControlBar.cpp
    Source/MainPanel.cpp
    Source/DrumsPanel.cpp
    Source/BassPanel.cpp
    Source/PianoPanel.cpp
    Source/GuitarPanel.cpp
    Source/drums/DrumEngine.cpp
    Source/bass/BassEngine.cpp
    Source/piano/PianoEngine.cpp
    Source/guitar/GuitarEngine.cpp
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

# ── QA: headless unit tests (JUCE UnitTest + CTest) ───────────────────────────
enable_testing()

juce_add_console_app(BridgeTests
    PRODUCT_NAME "BridgeTests"
    COMPANY_NAME "Bridge Audio"
    BUNDLE_ID "com.bridgeaudio.bridgetests"
)

juce_generate_juce_header(BridgeTests)

target_sources(BridgeTests PRIVATE
    Tests/BridgeTests_main.cpp
    Tests/LeaderStyleTests.cpp
    Tests/PresetConsistencyTests.cpp
    Tests/ProcessorStateTests.cpp
)

target_include_directories(BridgeTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(BridgeTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BridgeTests PRIVATE
    Bridge
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Linking the plugin static lib pulls JucePlugin_* compile defs; console app also sets JUCE_STANDALONE_APPLICATION.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    target_compile_options (BridgeTests PRIVATE "-Wno-macro-redefined")
endif()

add_test(NAME BridgeQA COMMAND BridgeTests)
set_tests_properties(BridgeQA PROPERTIES TIMEOUT 120)
