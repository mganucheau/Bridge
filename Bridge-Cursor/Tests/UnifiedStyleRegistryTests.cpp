#include <JuceHeader.h>
#include "BridgeInstrumentStyles.h"
#include "bass/BassStylePresets.h"

struct UnifiedStyleRegistryTests final : public juce::UnitTest
{
    UnifiedStyleRegistryTests()
        : juce::UnitTest ("Unified style index mapping", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("bridgeMelodicEngineStyleIndex wraps unified indices into engine table");

        const int unifiedN = bridgeUnifiedStyleCount();
        const int engineN = BassPreset::NUM_STYLES;
        expectGreaterThan (engineN, 0);

        for (int u = 0; u < unifiedN; ++u)
        {
            const int mapped = bridgeMelodicEngineStyleIndex (u);
            expect (mapped >= 0 && mapped < engineN);
            expectEquals (mapped, u % engineN);
        }
    }
};

static UnifiedStyleRegistryTests unifiedStyleRegistryTests;
