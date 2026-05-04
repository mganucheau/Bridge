#include <JuceHeader.h>
#include "drums/DrumEngine.h"
#include "drums/DrumsStylePresets.h"

struct DrumMorphRangeTests final : public juce::UnitTest
{
    DrumMorphRangeTests()
        : juce::UnitTest ("Drum morph: density 1 obeys activity budget in range (not 100% fill)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        DrumEngine e;
        e.setPatternLen (NUM_STEPS);
        e.setStyle (0);
        e.setSeed (1234);
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

        const int  total = (r1 - r0 + 1) * NUM_DRUMS;
        const double rate = (double) active / (double) juce::jmax (1, total);
        expect (rate < 0.55, "morph in sub-range should not fill every cell at d=1");
    }
};

static DrumMorphRangeTests drumMorphRangeTests;
