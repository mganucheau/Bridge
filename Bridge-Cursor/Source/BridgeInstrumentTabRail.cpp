#include "BridgeInstrumentTabRail.h"
#include "BridgeProcessor.h"

namespace
{
static const juce::Colour kAccents[5] = {
    juce::Colour (0xffd4af37), // LEA
    juce::Colour (0xffff6b4a), // DRU
    juce::Colour (0xff4ecdc4), // BAS
    juce::Colour (0xff9b7ede), // PIA
    juce::Colour (0xff5b9bd5), // GUI
};
} // namespace

BridgeInstrumentTabRail::BridgeInstrumentTabRail (BridgeProcessor& processor,
                                                  std::function<void (int)> onSelectTab)
    : proc (processor)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, true);

    struct TabSpec { int idx; const char* cap; const char* pid; const char* muteId; const char* soloId; };
    static const TabSpec specs[] = {
        { 0, "LEA", "leaderTabOn", "", "" },
        { 1, "DRU", "drumsOn", "mainMuteDrums", "mainSoloDrums" },
        { 2, "BAS", "bassOn", "mainMuteBass", "mainSoloBass" },
        { 3, "PIA", "pianoOn", "mainMutePiano", "mainSoloPiano" },
        { 4, "GUI", "guitarOn", "mainMuteGuitar", "mainSoloGuitar" },
    };

    for (const auto& s : specs)
    {
        tiles[(size_t) s.idx] = std::make_unique<BridgeInstrumentTabTile> (
            proc, s.idx, s.cap, s.pid, kAccents[(size_t) s.idx], onSelectTab, s.muteId, s.soloId);
        addAndMakeVisible (*tiles[(size_t) s.idx]);
    }
}

void BridgeInstrumentTabRail::paint (juce::Graphics&) {}

void BridgeInstrumentTabRail::resized()
{
    const auto r = getLocalBounds();
    constexpr int gap = kGapBetweenTiles;
    const int w = BridgeInstrumentTabTile::kButtonSide;
    int x = 0;
    for (int i = 0; i < 5; ++i)
    {
        if (tiles[(size_t) i] != nullptr)
            tiles[(size_t) i]->setBounds (x, 0, w, r.getHeight());
        x += w + gap;
    }
}

void BridgeInstrumentTabRail::setSelectedTab (int index)
{
    index = juce::jlimit (0, 4, index);
    for (int i = 0; i < 5; ++i)
        if (tiles[(size_t) i] != nullptr)
            tiles[(size_t) i]->setTileSelected (i == index);
}
