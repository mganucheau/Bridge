#include <JuceHeader.h>
#include "drums/DrumsStylePresets.h"
#include "bass/BassStylePresets.h"
#include "guitar/GuitarStylePresets.h"
#include "piano/PianoStylePresets.h"

struct StepGridConsistencyTest : public juce::UnitTest
{
    StepGridConsistencyTest()
        : juce::UnitTest ("Step grid / pattern length consistency", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("16 steps across engines");
        expectEquals (NUM_STEPS, 16);
        expectEquals (BassPreset::NUM_STEPS, NUM_STEPS);
        expectEquals (PianoPreset::NUM_STEPS, NUM_STEPS);
        expectEquals (GuitarPreset::NUM_STEPS, NUM_STEPS);
    }
};

static StepGridConsistencyTest stepGridConsistencyTest;
