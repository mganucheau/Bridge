#pragma once

#include <JuceHeader.h>

class BridgeProcessor;

/** Settings: General (theme, MIDI, ML status, about) + Personality (ML knobs). */
class BridgeSettingsDialog : public juce::Component
{
public:
    explicit BridgeSettingsDialog (BridgeProcessor& processor);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    BridgeProcessor& proc;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSettingsDialog)
};
