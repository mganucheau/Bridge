#pragma once

#include <JuceHeader.h>

/** Settings: MIDI device placeholders + app / copyright info. */
class BridgeSettingsDialog : public juce::Component
{
public:
    BridgeSettingsDialog();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label title { {}, "Bridge Settings" };
    juce::Label midiInLabel  { {}, "MIDI input" };
    juce::Label midiOutLabel { {}, "MIDI output" };
    juce::ComboBox midiInBox;
    juce::ComboBox midiOutBox;

    juce::Label aboutLabel;
    juce::TextEditor aboutBody;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSettingsDialog)
};
