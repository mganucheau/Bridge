#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "BridgeModelRunner.h"

class BridgeMLManager
{
public:
    BridgeMLManager();

    void loadModels();
    void reloadAllModels();

    void setExtraModelsSearchDirectory (juce::File dir) { extraModelsDir = std::move (dir); }
    void clearExtraModelsSearchDirectory() { extraModelsDir = juce::File(); }

    enum class ModelType
    {
        DrumHumanizer,
        BassModel,
        ChordsModel,
        MelodyModel,
        GuitarStrum,
        GuitarInertia
    };

    bool hasModel (ModelType type) const;

    /** Drums ONNX: 46 floats — see ml/training/drum_onnx_student.py. Output: 32 hit probabilities. */
    std::vector<float> generateDrums (const std::array<float, 10>& personalityKnobs,
                                      const std::array<float, 32>& priorHits,
                                      const std::array<float, 4>& grooveContext) const;

    /** Bass ONNX: 93 floats = 10 personality [0,1] + 16 kick + root/11, scale/9, style/7 + 64 context.
        Output: 64 floats (pitch, offset, vel, active per step) for mergePatternFromML. */
    std::vector<float> generateBass (const std::vector<float>& personalityKnobs,
                                       const std::vector<float>& kickPattern,
                                       int rootNote, int scaleType, int styleIndex,
                                       const std::vector<float>& bassContext) const;

    /** Chords ONNX: 37 floats — see ml/training/chords_onnx_student.py. Output: 24 floats. */
    std::vector<float> generateChords (const std::array<float, 10>& personalityKnobs,
                                         const std::array<float, 3>& tonality,
                                         const std::array<float, 8>& chordContext,
                                         const std::array<float, 16>& bassPattern) const;

    /** Melody ONNX: 45 floats — see ml/training/melody_onnx_student.py. Output: 32 floats. */
    std::vector<float> generateMelody (const std::array<float, 10>& personalityKnobs,
                                       const std::array<float, 3>& tonality,
                                       const std::array<float, 16>& noteContext,
                                       const std::array<float, 16>& rhythmicGrid) const;

    std::vector<float> generateStrumPattern (int styleIndex, int chordType, float density) const;

    float getGuitarInertiaDelay (int intervalSemitones) const;

private:
    std::unique_ptr<BridgeModelRunner> drumModel;
    std::unique_ptr<BridgeModelRunner> bassModel;
    std::unique_ptr<BridgeModelRunner> chordsModel;
    std::unique_ptr<BridgeModelRunner> melodyModel;
    std::unique_ptr<BridgeModelRunner> guitarStrumModel;
    std::unique_ptr<BridgeModelRunner> guitarInertiaModel;

    juce::File extraModelsDir;
};
