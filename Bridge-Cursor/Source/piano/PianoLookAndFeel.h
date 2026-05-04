#pragma once
#include <JuceHeader.h>
#include "BridgeLookAndFeel.h"

namespace PianoHIG
{
    inline const juce::Colour& surface                 = bridge::hig::secondaryGroupedBackground;
    inline const juce::Colour& surfaceDim              = bridge::hig::systemBackground;
    inline const juce::Colour& surfaceBright         = bridge::hig::tertiaryGroupedBackground;
    inline const juce::Colour& surfaceContainerLowest  = bridge::hig::systemBackground;
    inline const juce::Colour& surfaceContainerLow     = bridge::hig::secondaryGroupedBackground;
    inline const juce::Colour& surfaceContainer        = bridge::hig::secondaryGroupedBackground;
    inline const juce::Colour& surfaceContainerHigh    = bridge::hig::tertiaryGroupedBackground;
    inline const juce::Colour& surfaceContainerHighest = bridge::hig::quaternaryFill;

    inline const juce::Colour& onSurface               = bridge::hig::label;
    inline const juce::Colour& onSurfaceVariant        = bridge::hig::secondaryLabel;
    inline const juce::Colour& outline                 = bridge::hig::separatorOpaque;
    inline const juce::Colour& outlineVariant          = bridge::hig::quaternaryFill;

    inline juce::Colour primary()               { return bridge::colors::accentPiano(); }
    inline const juce::Colour& onPrimary        = bridge::hig::label;
    inline juce::Colour primaryContainer()     { return bridge::hig::accentFillSubdued (bridge::colors::accentPiano()); }
    inline const juce::Colour& onPrimaryContainer = bridge::hig::label;

    inline juce::Colour secondaryContainer()
    {
        return bridge::hig::tertiaryGroupedBackground.interpolatedWith (bridge::colors::accentPiano(), 0.32f);
    }
    inline const juce::Colour& onSecondaryContainer = bridge::hig::label;

    inline juce::Colour tertiary()               { return bridge::colors::accentPiano(); }
    inline juce::Colour tertiaryContainer()      { return bridge::hig::accentFillSubdued (bridge::colors::accentPiano()); }
    inline const juce::Colour& onTertiaryContainer = bridge::hig::label;

    inline juce::Colour stateHover   (juce::Colour base) { return bridge::hig::stateHover (base); }
    inline juce::Colour statePressed (juce::Colour base) { return bridge::hig::statePressed (base); }

    inline constexpr float cornerNone        = 0.0f;
    inline constexpr float cornerExtraSmall  = 0.0f;
    inline constexpr float cornerSmall       = 0.0f;
    inline constexpr float cornerMedium      = 0.0f;
    inline constexpr float cornerLarge       = 0.0f;
    inline constexpr float cornerExtraLarge  = 0.0f;

    inline void drawShadow (juce::Graphics& g, juce::Rectangle<float> bounds,
                            float cornerRadius, int elevationLevel)
    {
        bridge::hig::drawElevatedShadow (g, bounds, cornerRadius, elevationLevel);
    }

    inline void fillSurface (juce::Graphics& g, juce::Rectangle<float> bounds,
                             juce::Colour fill, float cornerRadius)
    {
        bridge::hig::fillRounded (g, bounds, fill, cornerRadius);
    }
}

// ─── Legacy alias tokens ─────────────────────────────────────────────────────
namespace PianoColors
{
    inline const juce::Colour& Background  = PianoHIG::surface;
    inline const juce::Colour& Panel       = PianoHIG::surfaceContainer;
    inline const juce::Colour& Border      = PianoHIG::outlineVariant;
    inline juce::Colour Accent() { return PianoHIG::primary(); }
    inline const juce::Colour& AccentBright= PianoHIG::onPrimaryContainer;
    inline const juce::Colour& TextPrimary = PianoHIG::onSurface;
    inline const juce::Colour& TextDim     = PianoHIG::onSurfaceVariant;
    inline juce::Colour ActiveStep() { return PianoHIG::primary(); }

    // Per-degree colours for the bass grid
    // 0=Root(amber), 1=m3(coral), 2=P4(mint), 3=P5(sky), 4=m7(violet), 5=Oct(gold), 6=Approach(pink), 7=Sub(deep)
    inline const juce::Colour DegreeColors[8] = {
        juce::Colour (0xFFFFB84D),  // Root   — amber (primary)
        juce::Colour (0xFFFF8A65),  // m3     — coral
        juce::Colour (0xFF80CBC4),  // P4     — teal
        juce::Colour (0xFF80D8FF),  // P5     — sky blue
        juce::Colour (0xFFCE93D8),  // m7     — soft violet
        juce::Colour (0xFFFFD54F),  // Oct    — bright gold
        juce::Colour (0xFFFF8A80),  // Approach — soft red
        juce::Colour (0xFF78909C),  // Sub    — steel grey
    };
}

// ─────────────────────────────────────────────────────────────────────────────
class PianoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PianoLookAndFeel()
    {
        using namespace PianoHIG;

        setColour (juce::ResizableWindow::backgroundColourId, surface);

        setColour (juce::Slider::thumbColourId,                primary());
        setColour (juce::Slider::trackColourId,                outlineVariant);
        setColour (juce::Slider::backgroundColourId,           surfaceContainerHigh);
        setColour (juce::Slider::rotarySliderFillColourId,     primary());
        setColour (juce::Slider::rotarySliderOutlineColourId,  outlineVariant);

        setColour (juce::Label::textColourId, onSurface);

        setColour (juce::TextButton::buttonColourId,   surfaceContainerHigh);
        setColour (juce::TextButton::buttonOnColourId, secondaryContainer());
        setColour (juce::TextButton::textColourOffId,  onSurfaceVariant);
        setColour (juce::TextButton::textColourOnId,   onSecondaryContainer);

        setColour (juce::ComboBox::backgroundColourId, surfaceContainerHigh);
        setColour (juce::ComboBox::textColourId,       onSurface);
        setColour (juce::ComboBox::outlineColourId,    outline);

        setColour (juce::PopupMenu::backgroundColourId,            surfaceContainerHigh);
        setColour (juce::PopupMenu::textColourId,                  onSurface);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, primaryContainer());
        setColour (juce::PopupMenu::highlightedTextColourId,       onPrimaryContainer);

        setColour (juce::ToggleButton::textColourId,       onSurfaceVariant);
        setColour (juce::ToggleButton::tickColourId,       primary());
        setColour (juce::ToggleButton::tickDisabledColourId, outlineVariant);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return bridge::hig::uiFont (13.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return bridge::hig::uiFont (13.0f, "Semibold");
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (box.getProperties()["bridgeInstrumentStrip"] || box.getProperties()["bridgeHeaderStrip"])
        {
            BridgeLookAndFeel().positionComboBoxText (box, label);
            return;
        }
        juce::LookAndFeel_V4::positionComboBoxText (box, label);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        using namespace PianoHIG;

        auto  bounds = juce::Rectangle<float> ((float)x, (float)y, (float)width, (float)height).reduced (6.0f);
        float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        float cx     = bounds.getCentreX();
        float cy     = bounds.getCentreY();

        // Track arc
        juce::Path track;
        track.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outlineVariant.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc
        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (primary());
        g.strokePath (valueArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob body
        float knobR = radius - 10.0f;
        auto knobBounds = juce::Rectangle<float> (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
        drawShadow (g, knobBounds, cornerExtraLarge, 1);
        fillSurface (g, knobBounds, surfaceContainerHighest, cornerExtraLarge);
        g.setColour (outline.withAlpha (0.35f));
        g.drawRoundedRectangle (knobBounds, cornerExtraLarge, 1.0f);

        // Handle dot
        float hx = cx + std::sin (angle) * (knobR * 0.55f);
        float hy = cy - std::cos (angle) * (knobR * 0.55f);
        g.setColour (primary());
        g.fillEllipse (hx - 4.0f, hy - 4.0f, 8.0f, 8.0f);

        juce::ignoreUnused (slider);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        using namespace PianoHIG;

        auto track = juce::Rectangle<float> ((float)x, (float)y, (float)width, (float)height);

        if (style == juce::Slider::LinearHorizontal)
        {
            auto r = track.withSize (track.getWidth(), 4.0f).withCentre (track.getCentre());
            fillSurface (g, r, surfaceContainerHighest, cornerExtraSmall);
            g.setColour (outline.withAlpha (0.35f));
            g.drawRoundedRectangle (r, cornerExtraSmall, 1.0f);

            float xFill = jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
            fillSurface (g, r.withRight (xFill), primaryContainer(), cornerExtraSmall);

            float thumbX = jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
            auto thumb = juce::Rectangle<float> (thumbX - 6.0f, r.getCentreY() - 10.0f, 12.0f, 20.0f);
            drawShadow (g, thumb, cornerSmall, 1);
            fillSurface (g, thumb, primary(), cornerSmall);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        if (box.getProperties()["bridgeInstrumentStrip"] || box.getProperties()["bridgeHeaderStrip"])
        {
            BridgeLookAndFeel().drawComboBox (g, width, height, isButtonDown, buttonX, buttonY, buttonW, buttonH, box);
            return;
        }

        using namespace PianoHIG;

        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float)width, (float)height);
        fillSurface (g, bounds.reduced (0.5f), surfaceContainerHigh, cornerSmall);
        g.setColour (outline.withAlpha (isButtonDown ? 0.9f : 0.55f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSmall, 1.0f);

        juce::Rectangle<int> arrowZone (buttonX, buttonY, buttonW, buttonH);
        juce::Path p;
        p.addTriangle ((float)arrowZone.getCentreX() - 4.0f, (float)arrowZone.getCentreY() - 2.0f,
                       (float)arrowZone.getCentreX() + 4.0f, (float)arrowZone.getCentreY() - 2.0f,
                       (float)arrowZone.getCentreX(),         (float)arrowZone.getCentreY() + 3.0f);
        g.setColour (onSurfaceVariant);
        g.fillPath (p);

        juce::ignoreUnused (box);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        using namespace PianoHIG;

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = cornerSmall;

        if (auto* tb = dynamic_cast<juce::TextButton*> (&button))
        {
            const auto text = tb->getButtonText();
            bool isPrimaryFilled = (text == "GEN" || text == "FILL");
            bool isToggleStyle   = tb->getClickingTogglesState();

            juce::Colour fill    = surfaceContainerHigh;
            juce::Colour stroke  = outline.withAlpha (0.35f);

            if (isPrimaryFilled)
            {
                fill   = primaryContainer();
                stroke = juce::Colours::transparentBlack;
            }
            else if (isToggleStyle && tb->getToggleState())
            {
                fill   = secondaryContainer();
                stroke = juce::Colours::transparentBlack;
            }
            else if (isToggleStyle && !tb->getToggleState())
            {
                fill   = surfaceContainer;
                stroke = outline.withAlpha (0.55f);
            }

            if (shouldDrawButtonAsDown)             fill = statePressed (fill);
            else if (shouldDrawButtonAsHighlighted) fill = stateHover   (fill);

            if (isPrimaryFilled || (isToggleStyle && tb->getToggleState()))
                drawShadow (g, bounds, rad, 1);

            fillSurface (g, bounds, fill, rad);
            g.setColour (stroke);
            g.drawRoundedRectangle (bounds, rad, 1.0f);

            // Draw text
            juce::Colour content = isPrimaryFilled          ? onPrimaryContainer
                                 : (isToggleStyle && tb->getToggleState()) ? onSecondaryContainer
                                 : onSurface;
            g.setColour (content);
            g.setFont   (bridge::hig::uiFont (13.0f, "Semibold"));
            g.drawFittedText (text, button.getLocalBounds(), juce::Justification::centred, 1);
            return;
        }

        juce::LookAndFeel_V4::drawButtonBackground (g, button, button.findColour (juce::TextButton::buttonColourId),
                                                    shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        // Text drawn in drawButtonBackground
        juce::ignoreUnused (g, button);
    }
};
