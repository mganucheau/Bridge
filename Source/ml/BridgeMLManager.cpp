#include "BridgeMLManager.h"
#include "BridgeModelBundle.h"
#include <JuceHeader.h>

#if BRIDGE_HAS_MODEL_DATA
#include <BinaryData.h>
#endif

namespace
{
static bool loadRunnerFromFileIfExists (BridgeModelRunner& runner, const juce::File& f, const char* logName)
{
    juce::ignoreUnused (logName);
    if (! f.existsAsFile())
        return false;
    return runner.loadFromFile (f.getFullPathName().toStdString());
}
} // namespace

BridgeMLManager::BridgeMLManager()
{
    drumModel = std::make_unique<BridgeModelRunner>();
    bassModel = std::make_unique<BridgeModelRunner>();
    chordsModel = std::make_unique<BridgeModelRunner>();
    melodyModel = std::make_unique<BridgeModelRunner>();
    guitarStrumModel = std::make_unique<BridgeModelRunner>();
    guitarInertiaModel = std::make_unique<BridgeModelRunner>();
}

void BridgeMLManager::reloadAllModels()
{
    drumModel = std::make_unique<BridgeModelRunner>();
    bassModel = std::make_unique<BridgeModelRunner>();
    chordsModel = std::make_unique<BridgeModelRunner>();
    melodyModel = std::make_unique<BridgeModelRunner>();
    guitarStrumModel = std::make_unique<BridgeModelRunner>();
    guitarInertiaModel = std::make_unique<BridgeModelRunner>();
    loadModels();
}

void BridgeMLManager::loadModels()
{
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_drum_humanizer_onnx)
    drumModel->loadFromMemory (BinaryData::drum_humanizer_onnx, (size_t) BinaryData::drum_humanizer_onnxSize, "drum_humanizer");
#endif
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_bass_model_onnx)
    bassModel->loadFromMemory (BinaryData::bass_model_onnx, (size_t) BinaryData::bass_model_onnxSize, "bass_model");
#endif
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_chords_model_onnx)
    chordsModel->loadFromMemory (BinaryData::chords_model_onnx, (size_t) BinaryData::chords_model_onnxSize, "chords_model");
#endif
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_melody_model_onnx)
    melodyModel->loadFromMemory (BinaryData::melody_model_onnx, (size_t) BinaryData::melody_model_onnxSize, "melody_model");
#endif
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_guitar_strum_onnx)
    guitarStrumModel->loadFromMemory (BinaryData::guitar_strum_onnx, (size_t) BinaryData::guitar_strum_onnxSize, "guitar_strum");
#endif
#if BRIDGE_HAS_MODEL_DATA && defined(BinaryData_guitar_inertia_onnx)
    guitarInertiaModel->loadFromMemory (BinaryData::guitar_inertia_onnx, (size_t) BinaryData::guitar_inertia_onnxSize, "guitar_inertia");
#endif

    juce::File binDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    juce::File modelsDir = binDir.getChildFile ("models");
    juce::File appModelsDir = BridgeModelBundle::getModelsDirectory();

    auto trySidecarAndExtraAndApp = [&] (BridgeModelRunner& runner, const juce::String& fileName, const char* logName)
    {
        if (! runner.isLoaded())
            loadRunnerFromFileIfExists (runner, modelsDir.getChildFile (fileName), logName);
        if (! runner.isLoaded() && extraModelsDir.isDirectory())
            loadRunnerFromFileIfExists (runner, extraModelsDir.getChildFile (fileName), logName);
        if (! runner.isLoaded())
            loadRunnerFromFileIfExists (runner, appModelsDir.getChildFile (fileName), logName);
    };

    trySidecarAndExtraAndApp (*drumModel, "drum_humanizer.onnx", "drum_humanizer");
    trySidecarAndExtraAndApp (*bassModel, "bass_model.onnx", "bass_model");
    trySidecarAndExtraAndApp (*chordsModel, "chords_model.onnx", "chords_model");
    trySidecarAndExtraAndApp (*melodyModel, "melody_model.onnx", "melody_model");
    trySidecarAndExtraAndApp (*guitarStrumModel, "guitar_strum.onnx", "guitar_strum");
    trySidecarAndExtraAndApp (*guitarInertiaModel, "guitar_inertia.onnx", "guitar_inertia");

    BridgeModelBundle::refreshLoadedVersionFromDisk();
}

bool BridgeMLManager::hasModel (ModelType type) const
{
    switch (type)
    {
        case ModelType::DrumHumanizer: return drumModel != nullptr && drumModel->isLoaded();
        case ModelType::BassModel: return bassModel != nullptr && bassModel->isLoaded();
        case ModelType::ChordsModel: return chordsModel != nullptr && chordsModel->isLoaded();
        case ModelType::MelodyModel: return melodyModel != nullptr && melodyModel->isLoaded();
        case ModelType::GuitarStrum: return guitarStrumModel != nullptr && guitarStrumModel->isLoaded();
        case ModelType::GuitarInertia: return guitarInertiaModel != nullptr && guitarInertiaModel->isLoaded();
    }
    return false;
}

namespace
{
static constexpr int kBassMlPersonality = 10;
static constexpr int kBassMlKick = 16;
static constexpr int kBassMlTonality = 3;
static constexpr int kBassMlContext = 64;
static constexpr int kBassMlInput = kBassMlPersonality + kBassMlKick + kBassMlTonality + kBassMlContext;

static constexpr int kDrumMlIn = 46;
static constexpr int kChordsMlIn = 37;
static constexpr int kMelodyMlIn = 45;
} // namespace

std::vector<float> BridgeMLManager::generateDrums (const std::array<float, 10>& personalityKnobs,
                                                     const std::array<float, 32>& priorHits,
                                                     const std::array<float, 4>& grooveContext) const
{
    if (! hasModel (ModelType::DrumHumanizer))
        return {};
    std::vector<float> in ((size_t) kDrumMlIn, 0.0f);
    for (int i = 0; i < 10; ++i)
        in[(size_t) i] = juce::jlimit (0.0f, 1.0f, personalityKnobs[(size_t) i]);
    for (int i = 0; i < 32; ++i)
        in[(size_t) (10 + i)] = juce::jlimit (0.0f, 1.0f, priorHits[(size_t) i]);
    for (int i = 0; i < 4; ++i)
        in[(size_t) (42 + i)] = juce::jlimit (0.0f, 1.0f, grooveContext[(size_t) i]);
    jassert (in.size() == (size_t) kDrumMlIn);
    auto out = drumModel->run (in);
    // Contract: 8×4 hit logits — see ml/training/drum_onnx_student.py
    if (out.size() != 32)
        return {};
    return out;
}

std::vector<float> BridgeMLManager::generateBass (const std::vector<float>& personalityKnobs,
                                                  const std::vector<float>& kickPattern,
                                                  int rootNote, int scaleType, int styleIndex,
                                                  const std::vector<float>& bassContext) const
{
    if (! hasModel (ModelType::BassModel))
        return {};
    std::vector<float> in ((size_t) kBassMlInput, 0.0f);
    for (int i = 0; i < kBassMlPersonality; ++i)
        in[(size_t) i] = juce::jlimit (0.0f, 1.0f,
                                       i < (int) personalityKnobs.size() ? personalityKnobs[(size_t) i] : 0.5f);
    for (int i = 0; i < kBassMlKick; ++i)
        in[(size_t) (kBassMlPersonality + i)] =
            i < (int) kickPattern.size() ? kickPattern[(size_t) i] : 0.0f;
    in[(size_t) (kBassMlPersonality + kBassMlKick + 0)] =
        static_cast<float> (juce::jlimit (0, 11, rootNote)) / 11.0f;
    in[(size_t) (kBassMlPersonality + kBassMlKick + 1)] =
        static_cast<float> (juce::jlimit (0, 9, scaleType)) / 9.0f;
    in[(size_t) (kBassMlPersonality + kBassMlKick + 2)] =
        static_cast<float> (juce::jlimit (0, 7, styleIndex)) / 7.0f;
    const int ctxBase = kBassMlPersonality + kBassMlKick + kBassMlTonality;
    for (int i = 0; i < kBassMlContext; ++i)
        in[(size_t) (ctxBase + i)] =
            i < (int) bassContext.size() ? bassContext[(size_t) i] : 0.0f;
    return bassModel->run (in);
}

std::vector<float> BridgeMLManager::generateChords (const std::array<float, 10>& personalityKnobs,
                                                    const std::array<float, 3>& tonality,
                                                    const std::array<float, 8>& chordContext,
                                                    const std::array<float, 16>& bassPattern) const
{
    if (! hasModel (ModelType::ChordsModel))
        return {};
    std::vector<float> in ((size_t) kChordsMlIn, 0.0f);
    for (int i = 0; i < 10; ++i)
        in[(size_t) i] = juce::jlimit (0.0f, 1.0f, personalityKnobs[(size_t) i]);
    for (int i = 0; i < 3; ++i)
        in[(size_t) (10 + i)] = juce::jlimit (0.0f, 1.0f, tonality[(size_t) i]);
    for (int i = 0; i < 8; ++i)
        in[(size_t) (13 + i)] = juce::jlimit (0.0f, 1.0f, chordContext[(size_t) i]);
    for (int i = 0; i < 16; ++i)
        in[(size_t) (21 + i)] = juce::jlimit (0.0f, 1.0f, bassPattern[(size_t) i]);
    jassert (in.size() == (size_t) kChordsMlIn);
    return chordsModel->run (in);
}

std::vector<float> BridgeMLManager::generateMelody (const std::array<float, 10>& personalityKnobs,
                                                    const std::array<float, 3>& tonality,
                                                    const std::array<float, 16>& noteContext,
                                                    const std::array<float, 16>& rhythmicGrid) const
{
    if (! hasModel (ModelType::MelodyModel))
        return {};
    std::vector<float> in ((size_t) kMelodyMlIn, 0.0f);
    for (int i = 0; i < 10; ++i)
        in[(size_t) i] = juce::jlimit (0.0f, 1.0f, personalityKnobs[(size_t) i]);
    for (int i = 0; i < 3; ++i)
        in[(size_t) (10 + i)] = juce::jlimit (0.0f, 1.0f, tonality[(size_t) i]);
    for (int i = 0; i < 16; ++i)
        in[(size_t) (13 + i)] = juce::jlimit (0.0f, 1.0f, noteContext[(size_t) i]);
    for (int i = 0; i < 16; ++i)
        in[(size_t) (29 + i)] = juce::jlimit (0.0f, 1.0f, rhythmicGrid[(size_t) i]);
    jassert (in.size() == (size_t) kMelodyMlIn);
    return melodyModel->run (in);
}

std::vector<float> BridgeMLManager::generateStrumPattern (int styleIndex, int chordType, float density) const
{
    if (! hasModel (ModelType::GuitarStrum))
        return {};
    std::vector<float> in (21, 0.0f);
    const int si = juce::jlimit (0, 7, styleIndex);
    in[(size_t) si] = 1.0f;
    const int ct = juce::jlimit (0, 11, chordType);
    in[8 + (size_t) ct] = 1.0f;
    in[20] = juce::jlimit (0.0f, 1.0f, density);
    return guitarStrumModel->run (in);
}

float BridgeMLManager::getGuitarInertiaDelay (int intervalSemitones) const
{
    if (! hasModel (ModelType::GuitarInertia))
        return 0.0f;
    const float x = static_cast<float> (juce::jlimit (0, 24, intervalSemitones)) / 24.0f;
    const auto out = guitarInertiaModel->run ({ x });
    if (out.empty())
        return 0.0f;
    return juce::jmax (0.0f, out[0]);
}
