#include <JuceHeader.h>
#include "LeaderStylePresets.h"

struct LeaderStylePresetTest : public juce::UnitTest
{
    LeaderStylePresetTest()
        : juce::UnitTest ("Leader arrangement presets", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("style count and names");
        expectEquals (NUM_LEADER_STYLES, 8);

        for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        {
            expect (LEADER_STYLE_NAMES[i] != nullptr);
            expect (juce::String (LEADER_STYLE_NAMES[i]).isNotEmpty());
        }

        beginTest ("bias table shape");
        const auto& balanced = LEADER_STYLE_BIASES[0];
        expectWithinAbsoluteError (balanced.hold, 0.f, 1.0e-5f);
        expectWithinAbsoluteError (balanced.tight, 0.f, 1.0e-5f);
        expectWithinAbsoluteError (balanced.unity, 0.f, 1.0e-5f);
        expectWithinAbsoluteError (balanced.breath, 0.f, 1.0e-5f);
        expectWithinAbsoluteError (balanced.spark, 0.f, 1.0e-5f);
    }
};

static LeaderStylePresetTest leaderStylePresetTest;
