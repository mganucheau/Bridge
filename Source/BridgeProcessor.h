#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include "drums/DrumEngine.h"
#include "bass/BassEngine.h"
#include "piano/PianoEngine.h"
#include "guitar/GuitarEngine.h"
#include "BridgeUpdateChecker.h"

class BridgeEditor;
class BridgeMLManager;

class BridgeProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener
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
        const auto& in = layouts.getMainInputChannelSet();
        if (in != juce::AudioChannelSet::disabled()
            && in != juce::AudioChannelSet::mono()
            && in != juce::AudioChannelSet::stereo())
            return false;

        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

        for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
            if (layouts.getChannelSet (false, bus) != juce::AudioChannelSet::disabled())
                return false;

        return true;
    }

    DrumEngine   drumEngine;
    BassEngine   bassEngine;
    PianoEngine  pianoEngine;
    GuitarEngine guitarEngine;

    juce::AudioProcessorValueTreeState apvtsMain;
    juce::AudioProcessorValueTreeState apvtsDrums;
    juce::AudioProcessorValueTreeState apvtsBass;
    juce::AudioProcessorValueTreeState apvtsPiano;
    juce::AudioProcessorValueTreeState apvtsGuitar;

    // UI tab: 0 Main, 1 Drums, 2 Bass, 3 Piano, 4 Guitar
    std::atomic<int> activeTab { 1 };

    // Last BPM the processor saw from the host (or fell back to). Read by the header BPM display.
    std::atomic<double> currentHostBpm { 120.0 };

    void triggerDrumsGenerate();
    void triggerDrumsFill (int fromStep = 12);
    void rebuildDrumsGridPreview();
    /** Panels / UI: push APVTS into engines (otherwise private to processor). */
    void syncDrumsEngineFromAPVTS();
    void syncBassEngineFromAPVTS();
    void syncPianoEngineFromAPVTS();
    void syncGuitarEngineFromAPVTS();
    const DrumPattern& getPatternForGrid() const;
    /** Global selection (1-based steps), always from apvtsMain; never collapsed to full clip. */
    void getMainSelectionBounds (int numSteps, int& loopStart, int& loopEnd) const;
    bool isMainSelectionFullClip (int numSteps) const;
    /** UI / grid highlight — same as main selection. */
    void getDrumsLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> drumsCurrentStep { -1 };
    std::atomic<int> drumsCurrentVisualStep { -1 };

    void triggerBassGenerate();
    void applyBassStyleAndRegenerate (int styleIndex);
    void triggerBassFill (int fromStep = 12);
    void rebuildBassGridPreview();
    void randomizeBassSettings();
    void getBassLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> bassCurrentStep { -1 };
    std::atomic<int> bassCurrentVisualStep { -1 };

    void triggerPianoGenerate();
    void applyPianoStyleAndRegenerate (int styleIndex);
    void triggerPianoFill (int fromStep = 12);
    void rebuildPianoGridPreview();
    void randomizePianoSettings();
    void getPianoLoopBounds (int& loopStart, int& loopEnd) const;
    std::atomic<int> pianoCurrentStep { -1 };
    std::atomic<int> pianoCurrentVisualStep { -1 };

    void triggerGuitarGenerate();
    void applyGuitarStyleAndRegenerate (int styleIndex);
    void triggerGuitarFill (int fromStep = 12);
    void rebuildGuitarGridPreview();
    void randomizeGuitarSettings();
    void getGuitarLoopBounds (int& loopStart, int& loopEnd) const;

    /** Leader / QA: morph every engine using main loop selection (1-based APVTS → 0-based engines). */
    void morphAllEnginesToMainSelection();

    /** Drums: sync from APVTS and regenerate the pattern (ML when loaded) for main selection / full bar. */
    void regenerateDrumsAfterKnobChange();

    BridgeMLManager* getMLManager() const noexcept { return mlManager.get(); }

    /** At most one automatic check per 24h (tracked in APVTS); silent on network failure. */
    void requestAutomaticModelUpdateCheckIfDue();
    void requestManualModelUpdateCheck();

    juce::String getPendingModelUpdateVersion() const noexcept { return mlPendingModelUpdateVersion; }
    juce::String getPendingModelUpdateUrl() const noexcept { return mlPendingModelUpdateUrl; }

    /** Preview note from UI (audio thread drains). */
    void queueMelodicPreviewNote (int midiChannel, int noteNumber, int velocity);

    /** Melodic panels register (chain) to refresh layout after root/scale/octave remap. */
    std::function<void()> onMelodicTonalityChanged;

    /** Fixed PPQ per pattern step: one 16th note (0.25 PPQ). Transport ignores header time division. */
    double getTransportPpqPerPatternStep() const noexcept;

#if BRIDGE_ENABLE_QA_HOOKS
    /** For BridgeTests: queue a bass note-off so UI rebuild + processBlock flush path can be observed. */
    void bridgeQaInjectBassPendingNoteOff (int channel, int noteNumber, int64 atSample);
    bool bridgeQaMelodicFlushPendingForTests() const noexcept;
#endif

    std::atomic<int> guitarCurrentStep { -1 };
    std::atomic<int> guitarCurrentVisualStep { -1 };

    void refreshBassKickHintFromDrums();
    void refreshChordsBassHintFromBass();
    /** Push drum step activity to bass/piano/guitar engines for follow-rhythm bias. */
    void publishDrumActivityToFollowers();
    /** Apply arrangement section macro (Intro..Outro × intensity) to a per-instrument
        (density, complexity) pair. Engine APVTS values remain the user's set point; this
        nudges them toward section-appropriate targets before they're pushed to engines. */
    void applyArrangementMacro (float& density, float& complexity, bool isDrums) const;

    /** Lerp instrument complexity/density toward stored targets for the chosen song section. */
    void startArrangementTransition (int section);
    /** Write current per-instrument XY into apvtsMain section targets (context menu). */
    void captureArrangementTargetsForSection (int section);
    /** -1 = idle; 0..1 = UI progress for arrangement transition (~1 s wall clock). */
    float getArrangementTransitionProgress() const noexcept;

private:
    void runDrumGenerateForCurrentMainSelection();
    void handleModelUpdateCheckResult (BridgeUpdateChecker::UpdateInfo info);

    void syncDrumsMLPersonalityToEngine();
    void syncChordsMLPersonalityToEngine();
    void syncMelodyMLPersonalityToEngine();

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void getDrumsPlaybackWrapBounds (int& loopStart, int& loopEnd) const;
    void getBassPlaybackWrapBounds (int& loopStart, int& loopEnd) const;
    void getPianoPlaybackWrapBounds (int& loopStart, int& loopEnd) const;
    void getGuitarPlaybackWrapBounds (int& loopStart, int& loopEnd) const;

    void clampMainLoopIntsToPhrase();

    bool playbackLoopEngaged() const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout buildMainLayout();
    static juce::AudioProcessorValueTreeState::ParameterLayout buildDrumsLayout();

    void randomizeDrumsSettings();

    void processDrumsBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processBassBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processPianoBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                             const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);
    void processGuitarBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                           const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples);

    /** Host-transport beat accumulator; mutates only main loop selection (1-based) inside engines. */
    void advanceJamClockAndMaybeEvolve (double bpm, int numSamples,
                                        bool drumsOn, bool bassOn, bool pianoOn, bool guitarOn);

    void scheduleDrumsNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                   int sampleOffset, juce::MidiBuffer& midi);
    void sendDrumsNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void scheduleBassNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                   int sampleOffset, juce::MidiBuffer& midi);
    void sendBassNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void schedulePianoNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                    int sampleOffset, juce::MidiBuffer& midi);
    void sendPianoNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    void scheduleGuitarNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                  int sampleOffset, juce::MidiBuffer& midi);
    void sendGuitarNoteOffs (juce::MidiBuffer& midi, int sampleOffset);

    bool isDrumsLaneAudible (int drum) const;
    static double drumsTickerRateForChoiceIndex (int choiceIndex);
    static double bassTickerRateForChoiceIndex (int choiceIndex);

    double sampleRate = 44100.0;
    int    samplesPerBlock = 512;
    int64  midiSampleClock = 0;

    struct PendingOff { int channel; int note; int64 atSample; };
    juce::Array<PendingOff> drumsPendingNoteOffs;
    juce::Array<PendingOff> bassPendingNoteOffs;
    juce::Array<PendingOff> pianoPendingNoteOffs;
    juce::Array<PendingOff> guitarPendingNoteOffs;

    std::atomic<bool> melodicPreviewFlushPending { false };
    bool pianoSustainLatchDown = false;
    int  lastPianoSustainCc64Value = -1;

    void flushPendingNoteOffs (juce::Array<PendingOff>& pending, juce::MidiBuffer& midi);
    void markMelodicPreviewFlushIfMessageThread();
    void servicePianoSustainPedal (juce::MidiBuffer& midi, bool phraseBoundary);

    double drumsLastBpm = 120.0;
    int    drumsLastProcessedStep = -1;
    // One-block forward look-ahead so anticipated (negative-shift) hits whose
    // nominal step boundary lies past the current block end can still be emitted
    // in this block. The mask remembers which drums of `…GlobalStep` were already
    // fired so the next block's main loop skips them and avoids double-strikes.
    int      drumsAnticipatedGlobalStep = INT_MIN;
    uint32_t drumsAnticipatedDrumMask   = 0;
    bool   drumsFillQueued = false;
    int    drumsFillFromStep = 12;
    double drumsJamDebtBeats = 0.0;
    double drumsTickerRate = 1.0;
    std::array<bool, NUM_DRUMS> drumsLaneMute {};
    std::array<bool, NUM_DRUMS> drumsLaneSolo {};
    bool drumsAnySolo = false;

    double bassLastBpm = 120.0;
    int    bassLastProcessedStep = -1;
    bool   bassFillQueued = false;
    int    bassFillFromStep = 12;
    double bassJamDebtBeats = 0.0;
    int    bassMidiChannel = 1;
    double bassTickerRate = 1.0;

    double pianoLastBpm = 120.0;
    int    pianoLastProcessedStep = -1;
    bool   pianoFillQueued = false;
    int    pianoFillFromStep = 12;
    double pianoJamDebtBeats = 0.0;
    int    pianoMidiChannel = 2;
    double pianoTickerRate = 1.0;

    double guitarLastBpm = 120.0;
    int    guitarLastProcessedStep = -1;
    bool   guitarFillQueued = false;
    int    guitarFillFromStep = 12;
    double guitarJamDebtBeats = 0.0;
    int    guitarMidiChannel = 3;
    double guitarTickerRate = 1.0;

    int drumsMidiChannel = 10;

    int lastSpanLockStart = 1;
    int lastSpanLockEnd = 16;
    bool spanLockApplying = false;

    struct PreviewNoteEvent
    {
        int channel = 1;
        int note = 60;
        int velocity = 100;
        int samplesRemaining = 0;
        bool noteOnSent = false;
    };
    juce::Array<PreviewNoteEvent> previewNotes;
    juce::CriticalSection previewLock;

    std::unique_ptr<BridgeMLManager> mlManager;
    BridgeUpdateChecker modelUpdateChecker;
    juce::String mlPendingModelUpdateVersion;
    juce::String mlPendingModelUpdateUrl;

    struct ArrangementLerpTimer;
    friend struct ArrangementLerpTimer;
    void arrangementTransitionTick();

    std::unique_ptr<ArrangementLerpTimer> arrangementLerp;
    std::atomic<float> arrangementLerpProgress { -1.0f };
    bool arrangementStateRestoreInProgress = false;
    float arrangementLerpTargets[8] {};
    float arrangementLerpCurrent[8] {};
    double arrangementTransitionStartMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeProcessor)
};
