#include <JuceHeader.h>
#include "drums/DrumEngine.h"
#include "drums/DrumsStylePresets.h"

namespace
{
int countHatSteps (const DrumEngine& e)
{
    int n = 0;
    for (int s = 0; s < NUM_STEPS; ++s)
        if (e.getStep (s)[2].active || e.getStep (s)[3].active)
            ++n;
    return n;
}

int countTomCrashSteps (const DrumEngine& e)
{
    int c = 0;
    for (int s = 0; s < NUM_STEPS; ++s)
        for (int d = 4; d <= 7; ++d)
            if (e.getStep (s)[(size_t) d].active)
                ++c;
    return c;
}

void configureHouseEngine (DrumEngine& e)
{
    e.setPatternLen (NUM_STEPS);
    e.setStyle (3); // House
    e.setDensity (0.55f);
    e.setHumanize (0.0f);
    e.setSwing (0.0f);
    e.setGhostAmount (0.0f);
    e.setHold (0.0f);
    e.setFillRate (0.0f);
    e.setTemperature (1.0f);
    e.setVelocity (0.85f);
    std::array<float, 10> ml {};
    e.setMLPersonalityKnobs (ml);
}
} // namespace

struct DrumsHouseComplexityMonotonicityTests final : public juce::UnitTest
{
    DrumsHouseComplexityMonotonicityTests()
        : juce::UnitTest ("Drums: House style complexity monotonicity (hats)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("House + complexity: hat activity rises, tom/crash stay controlled");
        constexpr int kSeeds = 400;
        int sumLow = 0, sumHigh = 0;
        int sumTomCrashLow = 0, sumTomCrashHigh = 0;
        int wins = 0;

        for (int i = 0; i < kSeeds; ++i)
        {
            const uint32_t seed = (uint32_t) (10007u + (uint32_t) i * 7919u);

            DrumEngine low;
            configureHouseEngine (low);
            low.setSeed (seed);
            low.setComplexity (0.12f);
            low.generatePattern (false, nullptr);
            const int hL = countHatSteps (low);
            const int tL = countTomCrashSteps (low);

            DrumEngine high;
            configureHouseEngine (high);
            high.setSeed (seed);
            high.setComplexity (0.88f);
            high.generatePattern (false, nullptr);
            const int hH = countHatSteps (high);
            const int tH = countTomCrashSteps (high);

            sumLow += hL;
            sumHigh += hH;
            sumTomCrashLow += tL;
            sumTomCrashHigh += tH;
            if (hH >= hL)
                ++wins;
        }

        const double meanLow  = (double) sumLow / (double) kSeeds;
        const double meanHigh = (double) sumHigh / (double) kSeeds;
        const double meanTomL = (double) sumTomCrashLow / (double) kSeeds;
        const double meanTomH = (double) sumTomCrashHigh / (double) kSeeds;

        expect (meanHigh > meanLow + 0.15,
                "Higher complexity should yield measurably more hat activity on average (House)");
        expect ((double) wins > (double) kSeeds * 0.62,
                "Per-seed hat step count should usually not drop when raising complexity");
        expect (meanTomH <= meanTomL + 2.5,
                "Tom/crash layers should not explode when complexity rises (House)");
    }
};

static DrumsHouseComplexityMonotonicityTests drumsHouseComplexityMonotonicityTests;
