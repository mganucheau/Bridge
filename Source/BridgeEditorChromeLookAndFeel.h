#pragma once

#include <JuceHeader.h>

/** Plugin header chrome: transport ShapeButtons, gear, host-sync TextButton (no extra focus fill). */
class BridgeEditorChromeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                        int buttonX, int buttonY, int buttonW, int buttonH,
                        juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

private:
    static juce::Colour chromeFieldBg();
    static juce::Colour chromeFieldHover();
    static juce::Colour chromeBorder();
};
