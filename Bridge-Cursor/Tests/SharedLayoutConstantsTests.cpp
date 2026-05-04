#include <JuceHeader.h>
#include "MelodicGridLayout.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"

struct SharedLayoutConstantsTests final : public juce::UnitTest
{
    SharedLayoutConstantsTests()
        : juce::UnitTest ("Shared layout constants (grid / shell)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("Melodic grid row count and loop strip height stay stable for UI + prompts");

        expectEquals (bridge::kMelodicOctaveRows, 12);
        expectEquals (bridge::kLoopRangeStripHeightPx, 18);

        beginTest ("splitInstrumentContent reserves bottom strip and gap");

        juce::Rectangle<int> content (0, 0, 960, 600);
        const int fullH = content.getHeight();
        auto shell = bridge::panelLayout::splitInstrumentContent (content, 8);
        expectGreaterThan (shell.mainCard.getHeight(), 0);
        expectEquals (shell.bottomStrip.getHeight(), bridge::instrumentLayout::kBottomCardH);
        expectEquals (shell.mainCard.getHeight() + bridge::instrumentLayout::kSectionGap
                          + shell.bottomStrip.getHeight() + 8,
                      fullH);

        beginTest ("Unified style registry is non-empty and matches STYLE_NAMES table size");

        const int n = bridgeUnifiedStyleCount();
        expectGreaterThan (n, 0);
        int nonEmpty = 0;
        for (int i = 0; i < n; ++i)
            if (juce::String (bridgeUnifiedStyleNames()[i]).trim().isNotEmpty())
                ++nonEmpty;
        expectEquals (nonEmpty, n);
    }
};

static SharedLayoutConstantsTests sharedLayoutConstantsTests;
