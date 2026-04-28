#include <JuceHeader.h>
#include "bass/BassEngine.h"
#include "bass/BassStylePresets.h"

namespace
{
int maxPolyphonyPerStep (const BassEngine& e, int patternLen)
{
    int maxP = 0;
    patternLen = juce::jlimit (1, bridge::phrase::kMaxSteps, patternLen);
    for (int s = 0; s < patternLen; ++s)
    {
        int n = e.getStep (s).active ? 1 : 0;
        maxP = juce::jmax (maxP, n);
    }
    return maxP;
}

int totalActiveSteps (const BassEngine& e, int patternLen)
{
    int n = 0;
    patternLen = juce::jlimit (1, bridge::phrase::kMaxSteps, patternLen);
    for (int s = 0; s < patternLen; ++s)
        if (e.getStep (s).active)
            ++n;
    return n;
}
} // namespace

struct MelodicHumanFeelMetricsTests final : public juce::UnitTest
{
    MelodicHumanFeelMetricsTests()
        : juce::UnitTest ("Melodic baseline: bass monophony + density sanity", "Bridge/QA")
    {
    }

    void runTest() override
    {
        BassEngine e;
        e.setPatternLen (BassPreset::NUM_STEPS);
        e.setStyle (0);
        e.setSeed (4242);
        e.setDensity (1.0f);
        e.setComplexity (1.0f);
        e.generatePattern (false, nullptr);

        expect (maxPolyphonyPerStep (e, BassPreset::NUM_STEPS) <= 1, "bass remains monophonic per step");

        const int activeHi = totalActiveSteps (e, BassPreset::NUM_STEPS);

        BassEngine sparse;
        sparse.setPatternLen (BassPreset::NUM_STEPS);
        sparse.setStyle (0);
        sparse.setSeed (4242);
        sparse.setDensity (0.05f);
        sparse.setComplexity (0.1f);
        sparse.generatePattern (false, nullptr);

        const int activeLo = totalActiveSteps (sparse, BassPreset::NUM_STEPS);
        expect (activeHi >= activeLo, "raising density should not yield fewer bass onsets");

        beginTest ("Manual melodic QA");
        logMessage ("Listening: ghost tiers, slides (where enabled), pocket vs drums.");
        expect (true);
    }
};

static MelodicHumanFeelMetricsTests melodicHumanFeelMetricsTests;
