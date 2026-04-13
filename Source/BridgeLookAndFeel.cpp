#include "BridgeLookAndFeel.h"

using namespace bridge::colors;

namespace
{
    const juce::Identifier knobStyleId ("knobStyle");
    const juce::Identifier knobAccentId ("knobAccent");
}

BridgeLookAndFeel::BridgeLookAndFeel()
{
    setColour (juce::Label::textColourId, textPrimary);
    setColour (juce::ComboBox::backgroundColourId, cardSurface);
    setColour (juce::ComboBox::textColourId, textSecondary);
    setColour (juce::ComboBox::outlineColourId, cardOutline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    
    setColour (juce::PopupMenu::backgroundColourId, cardSurface);
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cardOutline);
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary);

    setDefaultSansSerifTypefaceName ("Inter"); // Fallback to sans-serif OS font if missing
}

void BridgeLookAndFeel::setKnobStyle (juce::Slider& slider, KnobStyle style, juce::Colour accent)
{
    slider.getProperties().set (knobStyleId, juce::var((int) style));
    slider.getProperties().set (knobAccentId, juce::var((int) accent.getARGB()));
}

void BridgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, const float rotaryStartAngle,
                                          const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();
    float rx = centreX - radius;
    float ry = centreY - radius;
    float rw = radius * 2.0f;

    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    int styleInt = slider.getProperties().getWithDefault (knobStyleId, (int) KnobStyle::BigRing);
    KnobStyle style = (KnobStyle) styleInt;
    juce::Colour accentCol = juce::Colour ((juce::uint32) (int) slider.getProperties().getWithDefault (knobAccentId, (int) accentPiano.getARGB()));

    if (style == KnobStyle::BigRing)
    {
        // Draw the background thick track
        const float trackThickness = juce::jmax (radius * 0.22f, 2.0f);
        juce::Path trackPath;
        trackPath.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (knobTrack);
        g.strokePath (trackPath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw the filled colored arc
        juce::Path activePath;
        activePath.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                  0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentCol);
        g.strokePath (activePath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw the inner circle background
        const float innerRadius = radius - trackThickness - 4.0f;
        g.setColour (background);
        g.fillEllipse (centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
        
        // Draw the subtle top reflection or outline
        g.setColour (cardOutline.withAlpha(0.5f));
        g.drawEllipse (centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

        // Draw tick mark inside the dial
        juce::Path tick;
        const float tickL = innerRadius * 0.55f;
        tick.addLineSegment (juce::Line<float> (centreX, centreY - 4.0f, centreX, centreY - tickL), 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillPath (tick, juce::AffineTransform::rotation (angle, centreX, centreY));
    }
    else if (style == KnobStyle::SmallLane)
    {
        // Simple dot ring for lane height
        const float trackThickness = 2.0f;
        juce::Path track;
        track.addCentredArc (centreX, centreY, radius - 1.0f, radius - 1.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (background);
        g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centreX, centreY, radius - 1.0f, radius - 1.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentCol);
        g.strokePath (fill, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner circle
        g.setColour (cardSurface);
        g.fillEllipse (centreX - radius + 3.0f, centreY - radius + 3.0f, (radius - 3.0f) * 2.0f, (radius - 3.0f) * 2.0f);

        // Dot indicator
        juce::Path dot;
        dot.addEllipse (centreX - 1.5f, centreY - radius + 5.0f, 3.0f, 3.0f);
        g.setColour (accentCol);
        g.fillPath (dot, juce::AffineTransform::rotation (angle, centreX, centreY));
    }
}

void BridgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    float corner = 6.0f;

    auto baseColor = backgroundColour;
    if (shouldDrawButtonAsDown)
        baseColor = baseColor.brighter (0.1f);
    else if (shouldDrawButtonAsHighlighted)
        baseColor = baseColor.brighter (0.05f);

    g.setColour (baseColor);
    g.fillRoundedRectangle (bounds, corner);

    if (button.getToggleState() && button.getRadioGroupId() == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (bounds, corner);
    }
}

void BridgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setFont (button.getProperties().getWithDefault ("fontHeight", 11.0f));
    
    auto textCol = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    g.setColour (textCol);
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void BridgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 6.0f);
    
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced(0.5f), 6.0f, 1.0f);

    juce::Path arrow;
    arrow.addTriangle (0.0f, 0.0f, 8.0f, 0.0f, 4.0f, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow, juce::AffineTransform::translation ((float) width - 16.0f, (float) height * 0.5f - 2.5f));
}

void BridgeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 0, box.getWidth() - 30, box.getHeight());
    label.setFont (juce::Font(juce::FontOptions().withHeight(12.0f)));
    label.setColour (juce::Label::textColourId, textSecondary);
}
