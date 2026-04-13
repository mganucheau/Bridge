#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// Material Design 3 — dark colour scheme, amber/gold primary for Bass
// Primary seed: deep amber (#FF6F00 family) — warm, funky, bass energy
// ─────────────────────────────────────────────────────────────────────────────
namespace BassM3
{
    // Core surfaces (neutral dark)
    inline const juce::Colour surface                { 0xFF1C1A14 };
    inline const juce::Colour surfaceDim             { 0xFF141209 };
    inline const juce::Colour surfaceBright          { 0xFF2C2A1F };
    inline const juce::Colour surfaceContainerLowest { 0xFF100F07 };
    inline const juce::Colour surfaceContainerLow    { 0xFF1E1C14 };
    inline const juce::Colour surfaceContainer       { 0xFF231F16 };
    inline const juce::Colour surfaceContainerHigh   { 0xFF2D2A1E };
    inline const juce::Colour surfaceContainerHighest{ 0xFF38341F };

    // Content
    inline const juce::Colour onSurface              { 0xFFEAE2D5 };
    inline const juce::Colour onSurfaceVariant       { 0xFFCEC5AF };
    inline const juce::Colour outline                { 0xFF9A9080 };
    inline const juce::Colour outlineVariant         { 0xFF4D4738 };

    // Primary — deep amber / gold (bass energy)
    inline const juce::Colour primary                { 0xFFFFB84D };
    inline const juce::Colour onPrimary              { 0xFF3D2700 };
    inline const juce::Colour primaryContainer       { 0xFF5A3A00 };
    inline const juce::Colour onPrimaryContainer     { 0xFFFFDC9F };

    // Secondary — warm brown
    inline const juce::Colour secondaryContainer     { 0xFF4D3E28 };
    inline const juce::Colour onSecondaryContainer   { 0xFFEDDEC4 };

    // Tertiary — teal accent (contrast for note degree highlights)
    inline const juce::Colour tertiary               { 0xFF72D4C0 };
    inline const juce::Colour tertiaryContainer      { 0xFF2A4B44 };
    inline const juce::Colour onTertiaryContainer    { 0xFFBEF2E8 };

    // State overlays
    inline juce::Colour stateHover   (juce::Colour base) { return base.interpolatedWith (juce::Colours::white, 0.08f); }
    inline juce::Colour statePressed (juce::Colour base) { return base.interpolatedWith (juce::Colours::white, 0.12f); }

    // Shape tokens (corner radius)
    inline constexpr float cornerNone        = 0.0f;
    inline constexpr float cornerExtraSmall  = 4.0f;
    inline constexpr float cornerSmall       = 8.0f;
    inline constexpr float cornerMedium      = 12.0f;
    inline constexpr float cornerLarge       = 16.0f;
    inline constexpr float cornerExtraLarge  = 28.0f;

    inline void drawShadow (juce::Graphics& g, juce::Rectangle<float> bounds,
                            float cornerRadius, int elevationLevel)
    {
        if (elevationLevel <= 0 || cornerRadius <= 0.0f) return;

        const float yOff  = 1.0f + 0.6f  * (float)elevationLevel;
        const float alpha = jlimit (0.04f, 0.22f, 0.06f + 0.035f * (float)elevationLevel);

        juce::Path p;
        p.addRoundedRectangle (bounds.translated (0.0f, yOff), cornerRadius);
        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.fillPath (p);
    }

    inline void fillSurface (juce::Graphics& g, juce::Rectangle<float> bounds,
                             juce::Colour fill, float cornerRadius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);
    }
}

// ─── Legacy alias tokens ─────────────────────────────────────────────────────
namespace BassColors
{
    inline const juce::Colour& Background  = BassM3::surface;
    inline const juce::Colour& Panel       = BassM3::surfaceContainer;
    inline const juce::Colour& Border      = BassM3::outlineVariant;
    inline const juce::Colour& Accent      = BassM3::primary;
    inline const juce::Colour& AccentBright= BassM3::onPrimaryContainer;
    inline const juce::Colour& TextPrimary = BassM3::onSurface;
    inline const juce::Colour& TextDim     = BassM3::onSurfaceVariant;
    inline const juce::Colour& ActiveStep  = BassM3::primary;

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
class BassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BassLookAndFeel()
    {
        using namespace BassM3;

        setColour (juce::ResizableWindow::backgroundColourId, surface);

        setColour (juce::Slider::thumbColourId,                primary);
        setColour (juce::Slider::trackColourId,                outlineVariant);
        setColour (juce::Slider::backgroundColourId,           surfaceContainerHigh);
        setColour (juce::Slider::rotarySliderFillColourId,     primary);
        setColour (juce::Slider::rotarySliderOutlineColourId,  outlineVariant);

        setColour (juce::Label::textColourId, onSurface);

        setColour (juce::TextButton::buttonColourId,   surfaceContainerHigh);
        setColour (juce::TextButton::buttonOnColourId, secondaryContainer);
        setColour (juce::TextButton::textColourOffId,  onSurfaceVariant);
        setColour (juce::TextButton::textColourOnId,   onSecondaryContainer);

        setColour (juce::ComboBox::backgroundColourId, surfaceContainerHigh);
        setColour (juce::ComboBox::textColourId,       onSurface);
        setColour (juce::ComboBox::outlineColourId,    outline);

        setColour (juce::PopupMenu::backgroundColourId,            surfaceContainerHigh);
        setColour (juce::PopupMenu::textColourId,                  onSurface);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, primaryContainer);
        setColour (juce::PopupMenu::highlightedTextColourId,       onPrimaryContainer);

        setColour (juce::ToggleButton::textColourId,       onSurfaceVariant);
        setColour (juce::ToggleButton::tickColourId,       primary);
        setColour (juce::ToggleButton::tickDisabledColourId, outlineVariant);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions().withHeight (12.0f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Semibold"));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        using namespace BassM3;

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
        g.setColour (primary);
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
        g.setColour (primary);
        g.fillEllipse (hx - 4.0f, hy - 4.0f, 8.0f, 8.0f);

        juce::ignoreUnused (slider);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        using namespace BassM3;

        auto track = juce::Rectangle<float> ((float)x, (float)y, (float)width, (float)height);

        if (style == juce::Slider::LinearHorizontal)
        {
            auto r = track.withSize (track.getWidth(), 4.0f).withCentre (track.getCentre());
            fillSurface (g, r, surfaceContainerHighest, cornerExtraSmall);
            g.setColour (outline.withAlpha (0.35f));
            g.drawRoundedRectangle (r, cornerExtraSmall, 1.0f);

            float xFill = jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
            fillSurface (g, r.withRight (xFill), primaryContainer, cornerExtraSmall);

            float thumbX = jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
            auto thumb = juce::Rectangle<float> (thumbX - 6.0f, r.getCentreY() - 10.0f, 12.0f, 20.0f);
            drawShadow (g, thumb, cornerSmall, 1);
            fillSurface (g, thumb, primary, cornerSmall);
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
        using namespace BassM3;

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
        using namespace BassM3;

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
                fill   = primaryContainer;
                stroke = juce::Colours::transparentBlack;
            }
            else if (isToggleStyle && tb->getToggleState())
            {
                fill   = secondaryContainer;
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
            g.setFont   (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Semibold")));
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
