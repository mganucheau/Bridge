#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "piano/PianoStylePresets.h"

struct GridPatternParityTests final : public juce::UnitTest
{
    GridPatternParityTests()
        : juce::UnitTest ("Melodic grid matches committed pattern", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("piano: getPatternForGrid matches pattern for active steps");

        BridgeProcessor proc;
        for (int s = 0; s < PianoPreset::NUM_STEPS; ++s)
        {
            const auto& a = proc.pianoEngine.getStep (s);
            const auto& b = proc.pianoEngine.getPatternForGrid()[(size_t) s];
            expectEquals ((int) a.active, (int) b.active);
            if (a.active)
                expectEquals (a.midiNote, b.midiNote);
        }
    }
};

static GridPatternParityTests gridPatternParityTests;
