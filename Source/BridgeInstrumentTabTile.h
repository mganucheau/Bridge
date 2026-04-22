#pragma once

#include <JuceHeader.h>
#include <functional>

#include "BridgeProcessor.h"

/** Single 38×38 header tab (LEA / DRU / …): per-instrument accent; no LED/shortcuts. */
class BridgeInstrumentTabTile : public juce::Component,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    BridgeInstrumentTabTile (BridgeProcessor& processor,
                             int tabIndexInEditor,
                             const juce::String& shortCaption,
                             const juce::String& powerParameterId,
                             juce::Colour accentColour,
                             std::function<void (int)> onSelectTab);

    ~BridgeInstrumentTabTile() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setTileSelected (bool isSelectedTab);
    bool isTileSelected() const { return selected; }

    static constexpr int kButtonSide = 38;
    static constexpr int tabColumnWidth() noexcept { return kButtonSide; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    BridgeProcessor& proc;
    int tabIndex = 0;
    juce::String caption;
    juce::String powerParamId;
    juce::Colour accent;
    std::function<void (int)> onSelect;
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeInstrumentTabTile)
};
