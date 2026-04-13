#pragma once

#include <JuceHeader.h>

namespace bridge::colors
{
    const juce::Colour background        (0xFF141219);
    const juce::Colour cardSurface       (0xFF1B1823);
    const juce::Colour cardOutline       (0xFF2E2937);
    
    const juce::Colour textPrimary       (0xFFEAE2D5);
    const juce::Colour textSecondary     (0xFFB0A8C4);
    const juce::Colour textDim           (0xFF6A6578);

    const juce::Colour accentDrums       (0xFFE07A5A);
    const juce::Colour accentBass        (0xFF5CB8A8);
    const juce::Colour accentPiano       (0xFFB88CFF);
    const juce::Colour accentGuitar      (0xFF6EB3FF);
    const juce::Colour accentLeader      (0xFFD4A84B);

    const juce::Colour knobTrack         (0xFF25212E);
}

class BridgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BridgeLookAndFeel();
    ~BridgeLookAndFeel() override = default;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    // Custom configuration flag
    enum class KnobStyle
    {
        BigRing,
        SmallLane
    };

    static void setKnobStyle (juce::Slider& slider, KnobStyle style, juce::Colour accent);
};
