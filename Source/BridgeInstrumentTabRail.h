#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>

#include "BridgeInstrumentTabTile.h"

class BridgeProcessor;

/**
 * Right-hand instrument switcher in the editor header (LEA / DRU / BAS / PIA / GUI).
 * Lives in its own component so it can be added last and always stacked above other header chrome.
 */
class BridgeInstrumentTabRail : public juce::Component
{
public:
    BridgeInstrumentTabRail (BridgeProcessor& processor,
                             std::function<void (int)> onSelectTab);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSelectedTab (int index);

    BridgeInstrumentTabTile* getTile (int index) noexcept
    {
        if (index < 0 || index >= 5 || tiles[(size_t) index] == nullptr)
            return nullptr;
        return tiles[(size_t) index].get();
    }

    static constexpr int kGapBetweenTiles = 3;

    static constexpr int tabColumnWidth() noexcept { return BridgeInstrumentTabTile::tabColumnWidth(); }

    static constexpr int contentWidth() noexcept
    {
        return tabColumnWidth() * 5 + kGapBetweenTiles * 4;
    }

private:
    BridgeProcessor& proc;
    std::array<std::unique_ptr<BridgeInstrumentTabTile>, 5> tiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeInstrumentTabRail)
};
