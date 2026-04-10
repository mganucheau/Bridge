#pragma once

#include <JuceHeader.h>
#include "animal/DrumEngine.h"
#include "bootsy/BassEngine.h"
#include "stevie/PianoEngine.h"
#include "paul/GuitarEngine.h"

class BridgeEditor;

class BridgeProcessor : public juce::AudioProcessor
{
public:
    BridgeProcessor();
    ~BridgeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& data) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono()
            && out != juce::AudioChannelSet::stereo())
            return false;

        const auto& in = layouts.getMainInputChannelSet();
        return in == juce::AudioChannelSet::disabled()
            || in == juce::AudioChannelSet::mono()
            || in == juce::AudioChannelSet::stereo();
    }

    DrumEngine   drumEngine;
    BassEngine   bassEngine;
    PianoEngine  pianoEngine;
    GuitarEngine guitarEngine;

    juce::AudioProcessorValueTreeState apvtsMain;
    juce::AudioProcessorValueTreeState apvtsAnimal;
    juce::AudioProcessorValueTreeState apvtsBootsy;
    juce::AudioProcessorValueTreeState apvtsStevie;
    juce::AudioProcessorValueTreeState apvtsPaul;

    // UI tab: 0 Main, 1 Animal, 2 Bootsy, 3 Stevie, 4 Paul
    std::atomic<int> activeTab { 0 };

    void triggerAnimalGenerate();
    void triggerAnimalFill (int fromStep = 12);
    void rebuildAnimalGridPreview();
    const DrumPattern& getPatternForGrid() const;
    void getAnimalLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> animalCurrentStep { -1 };
    std::atomic<int> animalCurrentVisualStep { -1 };

    void triggerBootsyGenerate();
    void applyBootsyStyleAndRegenerate (int styleIndex);
    void triggerBootsyFill (int fromStep = 12);
    void rebuildBootsyGridPreview();
    void randomizeBootsySettings();
    void getBootsyLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> bootsyCurrentStep { -1 };
    std::atomic<int> bootsyCurrentVisualStep { -1 };

    void triggerStevieGenerate();
    void applyStevieStyleAndRegenerate (int styleIndex);
    void triggerStevieFill (int fromStep = 12);
    void rebuildStevieGridPreview();
    void randomizeStevieSettings();
    void getStevieLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> stevieCurrentStep { -1 };
    std::atomic<int> stevieCurrentVisualStep { -1 };

    void triggerPaulGenerate();
    void applyPaulStyleAndRegenerate (int styleIndex);
    void triggerPaulFill (int fromStep = 12);
    void rebuildPaulGridPreview();
    void randomizePaulSettings();
    void getPaulLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> paulCurrentStep { -1 };
    std::atomic<int> paulCurrentVisualStep { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout buildMainLayout();
    static juce::AudioProcessorValueTreeState::ParameterLayout buildAnimalLayout();

    void syncAnimalEngineFromAPVTS();
    void syncBootsyEngineFromAPVTS();
    void syncStevieEngineFromAPVTS();
    void syncPaulEngineFromAPVTS();
    void randomizeAnimalSettings();

    void processAnimalBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processBootsyBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processStevieBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processPaulBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                           const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);

    void scheduleAnimalNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                   int sampleOffset, juce::MidiBuffer& midi);
    void sendAnimalNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void scheduleBootsyNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                   int sampleOffset, juce::MidiBuffer& midi);
    void sendBootsyNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void scheduleStevieNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                    int sampleOffset, juce::MidiBuffer& midi);
    void sendStevieNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void schedulePaulNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                  int sampleOffset, juce::MidiBuffer& midi);
    void sendPaulNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    bool isAnimalLaneAudible (int drum) const;
    static double animalTickerRateForChoiceIndex (int choiceIndex);
    static double bootsyTickerRateForChoiceIndex (int choiceIndex);

    double sampleRate = 44100.0;
    int    samplesPerBlock = 512;
    int64  midiSampleClock = 0;

    struct PendingOff { int channel; int note; int64 atSample; };
    juce::Array<PendingOff> animalPendingNoteOffs;
    juce::Array<PendingOff> bootsyPendingNoteOffs;
    juce::Array<PendingOff> steviePendingNoteOffs;
    juce::Array<PendingOff> paulPendingNoteOffs;

    void flushPendingNoteOffs (juce::Array<PendingOff>& pending, juce::MidiBuffer& midi);

    double animalLastBpm = 120.0;
    int    animalLastProcessedStep = -1;
    bool   animalFillQueued = false;
    int    animalFillFromStep = 12;
    bool   animalPerformMode = false;
    double animalTickerRate = 1.0;
    std::array<bool, NUM_DRUMS> animalLaneMute {};
    std::array<bool, NUM_DRUMS> animalLaneSolo {};
    bool animalAnySolo = false;

    double bootsyLastBpm = 120.0;
    int    bootsyLastProcessedStep = -1;
    bool   bootsyFillQueued = false;
    int    bootsyFillFromStep = 12;
    int    bootsyMidiChannel = 1;
    double bootsyTickerRate = 1.0;

    double stevieLastBpm = 120.0;
    int    stevieLastProcessedStep = -1;
    bool   stevieFillQueued = false;
    int    stevieFillFromStep = 12;
    int    stevieMidiChannel = 2;
    double stevieTickerRate = 1.0;

    double paulLastBpm = 120.0;
    int    paulLastProcessedStep = -1;
    bool   paulFillQueued = false;
    int    paulFillFromStep = 12;
    int    paulMidiChannel = 3;
    double paulTickerRate = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeProcessor)
};
