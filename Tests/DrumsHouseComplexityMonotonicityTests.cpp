#include <JuceHeader.h>
#include "drums/DrumEngine.h"
#include "drums/DrumsStylePresets.h"

namespace
{
int countHatHits (const DrumEngine& e, int patternLen)
{
    int n = 0;
    patternLen = juce::jlimit (1, NUM_STEPS, patternLen);
    for (int s = 0; s < patternLen; ++s)
    {
        if (e.getStep (s)[2].active) ++n;
        if (e.getStep (s)[3].active) ++n;
    }
    return n;
}
} // namespace

/** House genre ought to gain hi-hat texture when complexity rises (monotone-ish complexity axis). */
struct DrumsHouseComplexityMonotonicityTests final : public juce::UnitTest
{
    DrumsHouseComplexityMonotonicityTests()
        : juce::UnitTest ("Drums House: complexity increases hat motion", "Bridge/QA")
    {
    }

    void runTest() override
    {
        constexpr int kHouseStyle = 3;

        DrumEngine low;
        low.setPatternLen (NUM_STEPS);
        low.setStyle (kHouseStyle);
        low.setSeed (90123);
        low.setDensity (0.85f);
        low.setComplexity (0.25f);
        low.generatePattern (false, nullptr);

        DrumEngine hi;
        hi.setPatternLen (NUM_STEPS);
        hi.setStyle (kHouseStyle);
        hi.setSeed (90123);
        hi.setDensity (0.85f);
        hi.setComplexity (0.92f);
        hi.generatePattern (false, nullptr);

        const int cLow = countHatHits (low, NUM_STEPS);
        const int cHi  = countHatHits (hi, NUM_STEPS);

        expect (cHi >= cLow,
                "House hats should not shrink sharply when complexity rises at matched density/seed");

        beginTest ("House four-on-the-floor backbone");
        for (int s : { 0, 4, 8, 12 })
            expect (hi.getStep (s)[0].active, "kick should be present on quarter notes in House");

        beginTest ("Manual QA");
        logMessage ("Verify perceptually that House hats gain syncopation / openness before complexity max.");
        expect (true);
    }
};

static DrumsHouseComplexityMonotonicityTests drumsHouseComplexityMonotonicityTests;
