#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeQaTestHelpers.h"

struct ProcessorMidiSustainTests final : public juce::UnitTest
{
    ProcessorMidiSustainTests()
        : juce::UnitTest ("Processor MIDI: piano sustain CC64", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("High sustain emits CC64 127 on piano MIDI channel when transport runs");

        BridgeProcessor proc;
        BridgeQaFakePlayHead playHead;
        playHead.playing = true;
        playHead.bpm = 120.0;
        playHead.ppqPosition = 0.0;
        proc.setPlayHead (&playHead);

        proc.prepareToPlay (44100.0, 512);
        bridgeQaSetFloatParam01 (proc.apvtsPiano, "sustain", 1.0f);

        const int pianoCh = (int) *proc.apvtsPiano.getRawParameterValue ("midiChannel");

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        expectGreaterThan (bridgeQaCountControllerValue (midi, pianoCh, 64, 127), 0);
    }
};

static ProcessorMidiSustainTests processorMidiSustainTests;

#if BRIDGE_ENABLE_QA_HOOKS

struct ProcessorMelodicFlushQaTests final : public juce::UnitTest
{
    ProcessorMelodicFlushQaTests()
        : juce::UnitTest ("Processor QA: melodic preview flush drains bass pending note-offs", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("rebuildBassGridPreview on message thread arms flush; processBlock emits injected note-off");

        juce::ScopedJuceInitialiser_GUI gui;

        BridgeProcessor proc;
        BridgeQaFakePlayHead playHead;
        playHead.playing = true;
        playHead.bpm = 120.0;
        playHead.ppqPosition = 4.0;
        proc.setPlayHead (&playHead);

        proc.prepareToPlay (44100.0, 512);

        proc.bridgeQaInjectBassPendingNoteOff (1, 55, (juce::int64) 1 << 30);
        expect (! proc.bridgeQaMelodicFlushPendingForTests());

        proc.rebuildBassGridPreview();
        expect (proc.bridgeQaMelodicFlushPendingForTests());

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        expect (! proc.bridgeQaMelodicFlushPendingForTests());
        expect (bridgeQaHasNoteOff (midi, 1, 55));
    }
};

static ProcessorMelodicFlushQaTests processorMelodicFlushQaTests;

#endif
