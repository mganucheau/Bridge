#include "BridgeLookAndFeel.h"
#include "BridgeInstrumentStripStyle.h"

namespace
{
    const juce::Identifier knobStyleId ("knobStyle");
    const juce::Identifier knobAccentId ("knobAccent");
}

BridgeLookAndFeel::BridgeLookAndFeel()
{
    setColour (juce::Label::textColourId, bridge::colors::textPrimary());
    setColour (juce::ComboBox::backgroundColourId, bridge::colors::cardSurface());
    setColour (juce::ComboBox::textColourId, bridge::colors::textSecondary());
    setColour (juce::ComboBox::outlineColourId, bridge::colors::cardOutline());
    setColour (juce::ComboBox::arrowColourId, bridge::colors::textDim());
    
    setColour (juce::PopupMenu::backgroundColourId, bridge::colors::cardSurface());
    setColour (juce::PopupMenu::textColourId, bridge::colors::textPrimary());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, bridge::colors::cardOutline());
    setColour (juce::PopupMenu::highlightedTextColourId, bridge::colors::textPrimary());

#if JUCE_MAC || JUCE_IOS
    setDefaultSansSerifTypefaceName (".AppleSystemUIFont");
#else
    setDefaultSansSerifTypefaceName (juce::Font::getDefaultSansSerifFontName());
#endif
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
    juce::Colour accentCol = juce::Colour ((juce::uint32) (int) slider.getProperties().getWithDefault (knobAccentId, (int) bridge::colors::accentPiano().getARGB()));

    if (style == KnobStyle::BigRing)
    {
        // Draw the background thick track
        const float trackThickness = juce::jmax (radius * 0.22f, 2.0f);
        juce::Path trackPath;
        trackPath.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (bridge::colors::knobTrack());
        g.strokePath (trackPath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw the filled colored arc
        juce::Path activePath;
        activePath.addCentredArc (centreX, centreY, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                  0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentCol);
        g.strokePath (activePath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw the inner circle background
        const float innerRadius = radius - trackThickness - 4.0f;
        g.setColour (bridge::colors::background());
        g.fillEllipse (centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
        
        // Draw the subtle top reflection or outline
        g.setColour (bridge::colors::cardOutline().withAlpha (0.5f));
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
        g.setColour (bridge::colors::background());
        g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centreX, centreY, radius - 1.0f, radius - 1.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentCol);
        g.strokePath (fill, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner circle
        g.setColour (bridge::colors::cardSurface());
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
    const auto stripTag = button.getProperties()["bridgeStripTag"].toString();

    if (stripTag == "loop" || stripTag == "spanLock")
    {
        const bool on = button.getToggleState();
        juce::Colour fill = bridge::instrumentStripStyle::fieldBg();
        juce::Colour border = juce::Colour (0xff444444);

        if (on)
        {
            fill = bridge::instrumentStripStyle::goldAccent();
            border = bridge::instrumentStripStyle::goldAccent();
        }

        if (! on && (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown))
            fill = bridge::instrumentStripStyle::fieldHoverBg();

        g.setColour (fill);
        g.fillRect (bounds);
        g.setColour (border);
        g.drawRect (bounds.reduced (1.0f), 2);
        return;
    }

    const auto ms = button.getProperties()["bridgeStripMs"].toString();

    if (ms.isNotEmpty())
    {
        const bool on = button.getToggleState();
        juce::Colour fill = bridge::instrumentStripStyle::fieldBg();
        juce::Colour border = juce::Colour (0xff444444);

        if (ms == "mute")
        {
            if (on)
            {
                fill = bridge::instrumentStripStyle::muteOn();
                border = bridge::instrumentStripStyle::muteOn();
            }
        }
        else if (ms == "solo")
        {
            if (on)
            {
                fill = bridge::instrumentStripStyle::soloOn();
                border = bridge::instrumentStripStyle::soloOn();
            }
        }

        if (! on && (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown))
            fill = bridge::instrumentStripStyle::fieldHoverBg();

        g.setColour (fill);
        g.fillRect (bounds);
        g.setColour (border);
        g.drawRect (bounds.reduced (1.0f), 2);
        return;
    }

    auto baseColor = backgroundColour;
    if (shouldDrawButtonAsDown)
        baseColor = baseColor.brighter (0.1f);
    else if (shouldDrawButtonAsHighlighted)
        baseColor = baseColor.brighter (0.05f);

    g.setColour (baseColor);
    g.fillRect (bounds);

    if (button.getToggleState() && button.getRadioGroupId() == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (bounds);
    }
}

void BridgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    const auto stripTag = button.getProperties()["bridgeStripTag"].toString();
    if (button.getProperties()["bridgeStripMs"].toString().isNotEmpty()
        || stripTag == "loop" || stripTag == "spanLock")
    {
        g.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
        g.setColour (juce::Colours::white);
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    g.setFont (button.getProperties().getWithDefault ("fontHeight", 11.0f));

    auto textCol = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    g.setColour (textCol);
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void BridgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    const bool strip = box.getProperties()["bridgeInstrumentStrip"]
                       || box.getProperties()["bridgeHeaderStrip"];

    juce::Colour fill = box.findColour (juce::ComboBox::backgroundColourId);
    if (strip && box.isMouseOver (true))
        fill = bridge::instrumentStripStyle::fieldHoverBg();

    g.setColour (fill);
    g.fillRect (bounds);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRect (bounds.reduced (0.5f), 1.0f);

    juce::Path arrow;
    arrow.addTriangle (0.0f, 0.0f, 8.0f, 0.0f, 4.0f, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow, juce::AffineTransform::translation ((float) width - 14.0f, (float) height * 0.5f - 2.5f));
}

void BridgeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    if (box.getProperties()["bridgeInstrumentStrip"] || box.getProperties()["bridgeHeaderStrip"])
    {
        label.setBounds (8, 0, box.getWidth() - 30, box.getHeight());
        label.setFont (bridge::hig::uiFont (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        return;
    }

    label.setBounds (12, 0, box.getWidth() - 32, box.getHeight());
    label.setFont (bridge::hig::uiFont (13.0f));
    label.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
}

void BridgeLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                       bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (scrollbar, isMouseOver, isMouseDown);

    const auto track = juce::Rectangle<int> (x, y, width, height).toFloat();
    g.setColour (bridge::colors::knobTrack().withAlpha (0.55f));
    g.fillRoundedRectangle (track, 4.0f);

    if (thumbSize <= 0)
        return;

    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { (float) x + 2.0f, (float) thumbStartPosition, (float) width - 4.0f, (float) thumbSize };
    else
        thumb = { (float) thumbStartPosition, (float) y + 2.0f, (float) thumbSize, (float) height - 4.0f };

    g.setColour (bridge::colors::textDim().withAlpha (0.9f));
    g.fillRoundedRectangle (thumb, 4.0f);
}
