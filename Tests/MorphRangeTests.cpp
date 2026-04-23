#include <JuceHeader.h>
#include "drums/DrumEngine.h"
#include "drums/DrumsStylePresets.h"

struct DrumMorphRangeTests final : public juce::UnitTest
{
    DrumMorphRangeTests()
        : juce::UnitTest ("Drum morph: density 1 fills every lane in range", "Bridge/QA")
    {
    }

    void runTest() override
    {
        DrumEngine e;
        e.setPatternLen (NUM_STEPS);
        e.setDensity (0.0f);
        e.morphPatternForDensityAndComplexity (0, NUM_STEPS - 1);

        e.setDensity (1.0f);
        const int r0 = 3;
        const int r1 = 5;
        e.morphPatternForDensityAndComplexity (r0, r1);

        int active = 0;
        for (int s = r0; s <= r1; ++s)
            for (int d = 0; d < NUM_DRUMS; ++d)
                if (e.getStep (s)[(size_t) d].active)
                    ++active;

        const int expected = (r1 - r0 + 1) * NUM_DRUMS;
        expectEquals (active, expected);
    }
};

static DrumMorphRangeTests drumMorphRangeTests;
