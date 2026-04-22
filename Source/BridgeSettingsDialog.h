#pragma once

#include <JuceHeader.h>

class BridgeProcessor;

/** Settings: theme, MIDI routing note, placeholders. */
class BridgeSettingsDialog : public juce::Component
{
public:
    explicit BridgeSettingsDialog (BridgeProcessor& processor);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    BridgeProcessor& proc;

    juce::Label title { {}, "Bridge Settings" };
    juce::Label themeLabel { {}, "UI theme" };
    juce::ComboBox themeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> themeAttach;

    juce::Label midiRoutingLabel { {}, "MIDI outputs" };
    juce::TextEditor midiRoutingBody;

    juce::Label aboutLabel;
    juce::TextEditor aboutBody;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSettingsDialog)
};
