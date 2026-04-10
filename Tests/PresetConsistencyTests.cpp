#include <JuceHeader.h>
#include "animal/AnimalStylePresets.h"
#include "bootsy/BootsyStylePresets.h"
#include "paul/PaulStylePresets.h"
#include "stevie/StevieStylePresets.h"

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
        expectEquals (BootsyPreset::NUM_STEPS, NUM_STEPS);
        expectEquals (SteviePreset::NUM_STEPS, NUM_STEPS);
        expectEquals (PaulPreset::NUM_STEPS, NUM_STEPS);
    }
};

static StepGridConsistencyTest stepGridConsistencyTest;
