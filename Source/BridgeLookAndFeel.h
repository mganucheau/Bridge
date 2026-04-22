#pragma once

#include <JuceHeader.h>
#include "BridgeAppleHIG.h"
#include "BridgeTheme.h"

namespace bridge::colors
{
    /** Window / canvas — driven by apvtsMain uiTheme (see BridgeTheme.cpp). */
    inline juce::Colour background()    { return bridge::theme::background(); }
    inline juce::Colour cardSurface()   { return bridge::theme::cardSurface(); }
    inline juce::Colour cardOutline()   { return bridge::theme::cardOutline(); }
    inline juce::Colour textPrimary()   { return bridge::theme::textPrimary(); }
    inline juce::Colour textSecondary() { return bridge::theme::textSecondary(); }
    inline juce::Colour textDim()       { return bridge::theme::textDim(); }
    inline juce::Colour knobTrack()     { return bridge::theme::knobTrack(); }

    inline juce::Colour accentLeader()  { return bridge::theme::accentLeader(); }
    inline juce::Colour accentDrums()   { return bridge::theme::accentDrums(); }
    inline juce::Colour accentBass()    { return bridge::theme::accentBass(); }
    inline juce::Colour accentPiano()   { return bridge::theme::accentPiano(); }
    inline juce::Colour accentGuitar()  { return bridge::theme::accentGuitar(); }
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
