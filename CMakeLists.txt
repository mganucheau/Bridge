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

# When ON, BridgeProcessor exposes small hooks used only by BridgeTests (pending-note flush, etc.).
# Set OFF for a shipping build that must not carry test-only entry points.
option(BRIDGE_ENABLE_QA_HOOKS "Expose QA-only hooks in BridgeProcessor for automated tests" ON)

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
    Source/BridgeModelBundle.cpp
    Source/BridgeUpdateChecker.cpp
    Source/BridgeHeaderBar.cpp
    Source/BridgeEditorChromeLookAndFeel.cpp
    Source/BridgeInstrumentTabTile.cpp
    Source/BridgeInstrumentTabRail.cpp
    Source/BridgeBottomHalf.cpp
    Source/BridgeLoopRangeStrip.cpp
    Source/BridgeVelocityStrip.cpp
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
    Source/ml/BridgeModelRunner.cpp
    Source/ml/BridgeMLManager.cpp
)

set(BRIDGE_UPDATE_MANIFEST_URL "https://updates.bridgeapp.io/models/manifest.json" CACHE STRING "Bridge model update manifest (JSON)")
target_compile_definitions(Bridge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        BRIDGE_APP_VERSION=9
        "BRIDGE_UPDATE_MANIFEST_URL=\"${BRIDGE_UPDATE_MANIFEST_URL}\""
)
if (BRIDGE_ENABLE_QA_HOOKS)
    target_compile_definitions(Bridge PRIVATE BRIDGE_ENABLE_QA_HOOKS=1)
endif()

target_link_libraries(Bridge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_events
        juce::juce_cryptography
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Optional: ONNX Runtime (ML inference) ─────────────────────────────────────
set(ORT_ROOT "${CMAKE_SOURCE_DIR}/libs/onnxruntime")

if(EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")
    target_include_directories(Bridge PRIVATE "${ORT_ROOT}/include")
    if(APPLE)
        target_link_libraries(Bridge PRIVATE "${ORT_ROOT}/lib/libonnxruntime.dylib")
    elseif(WIN32)
        target_link_libraries(Bridge PRIVATE "${ORT_ROOT}/lib/onnxruntime.lib")
    endif()
    target_compile_definitions(Bridge PRIVATE BRIDGE_ML_ENABLED=1)
    message(STATUS "Bridge: ONNX Runtime found — ML features enabled")
else()
    target_compile_definitions(Bridge PRIVATE BRIDGE_ML_ENABLED=0)
    message(STATUS "Bridge: ONNX Runtime not found — ML features disabled, using legacy humanization")
endif()

# ── Optional: embedded .onnx models (JUCE BinaryData) ─────────────────────────
set(MODEL_SOURCES "")
foreach(MODEL drum_humanizer bass_model chords_model melody_model guitar_strum guitar_inertia)
    if(EXISTS "${CMAKE_SOURCE_DIR}/ml/models/${MODEL}.onnx")
        list(APPEND MODEL_SOURCES "${CMAKE_SOURCE_DIR}/ml/models/${MODEL}.onnx")
        string(TOUPPER "${MODEL}" MODEL_UPPER)
        target_compile_definitions(Bridge PRIVATE "BinaryData_${MODEL}_onnx=1")
        message(STATUS "Bridge: Found model ${MODEL}.onnx")
    endif()
endforeach()

if(MODEL_SOURCES)
    juce_add_binary_data(BridgeMLModels SOURCES ${MODEL_SOURCES})
    target_link_libraries(Bridge PRIVATE BridgeMLModels)
    target_compile_definitions(Bridge PRIVATE BRIDGE_HAS_MODEL_DATA=1)
else()
    target_compile_definitions(Bridge PRIVATE BRIDGE_HAS_MODEL_DATA=0)
    message(STATUS "Bridge: No .onnx models found in ml/models/ — skipping binary data")
endif()

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
    Tests/ApvtsLayoutRegistryTests.cpp
    Tests/StateMigrationTests.cpp
    Tests/ProcessorMidiAndFlushTests.cpp
    Tests/MLModelSmokeTests.cpp
    Tests/TonalityEngineSyncTests.cpp
    Tests/GridPatternParityTests.cpp
    Tests/SharedLayoutConstantsTests.cpp
    Tests/UnifiedStyleRegistryTests.cpp
    Tests/MorphRangeTests.cpp
    Tests/DrumHumanFeelMetricsTests.cpp
)
if (BRIDGE_ENABLE_QA_HOOKS)
    target_compile_definitions(BridgeTests PRIVATE BRIDGE_ENABLE_QA_HOOKS=1)
endif()

target_include_directories(BridgeTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(BridgeTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BridgeTests PRIVATE
    Bridge
    $<TARGET_NAME_IF_EXISTS:BridgeMLModels>
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

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(test-ml
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/ml/training/snapshot_tests.py" run
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Bridge ML ONNX snapshot regression (no snapshots => pass)"
    )
    set(ML_REGISTER_VERSION "v1" CACHE STRING "Version tag for the ml-register target (snapshot ml/models into ml/models/<tag>)")
    add_custom_target(ml-register
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/ml/training/model_registry.py" register ${ML_REGISTER_VERSION}
                --source "${CMAKE_SOURCE_DIR}/ml/models" --root "${CMAKE_SOURCE_DIR}/ml/models"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Register ML ONNX set as ml/models/<ML_REGISTER_VERSION> (configure with -DML_REGISTER_VERSION=...)"
    )
    set(ML_BUNDLE_VERSION "v1" CACHE STRING "Version tag for ml-bundle (.bridgemodels output name)")
    add_custom_target(ml-bundle
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/ml/training/bundle.py" create
                --models-dir "${CMAKE_SOURCE_DIR}/ml/models"
                --version ${ML_BUNDLE_VERSION}
                --out "${CMAKE_SOURCE_DIR}/dist/${ML_BUNDLE_VERSION}.bridgemodels"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Package ml/models into dist/${ML_BUNDLE_VERSION}.bridgemodels"
    )
endif()
