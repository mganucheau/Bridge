#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// Material Design 3 — dark colour scheme (tonal roles, “Material You”-style)
// Reference: https://m3.material.io/styles/color/the-color-system/color-roles
// Values are aligned with the M3 dark-theme palette for a violet primary seed.
// ─────────────────────────────────────────────────────────────────────────────
namespace AnimalM3
{
    // Core surfaces
    inline const juce::Colour surface                { 0xFF1C1B1F };
    inline const juce::Colour surfaceDim             { 0xFF141218 };
    inline const juce::Colour surfaceBright            { 0xFF2B2930 };
    inline const juce::Colour surfaceContainerLowest { 0xFF0F0D13 };
    inline const juce::Colour surfaceContainerLow    { 0xFF1D1B20 };
    inline const juce::Colour surfaceContainer       { 0xFF211F26 };
    inline const juce::Colour surfaceContainerHigh     { 0xFF2B2930 };
    inline const juce::Colour surfaceContainerHighest  { 0xFF36343B };

    // Content
    inline const juce::Colour onSurface              { 0xFFE6E1E9 };
    inline const juce::Colour onSurfaceVariant       { 0xFFCAC4D0 };
    inline const juce::Colour outline                { 0xFF948F99 };
    inline const juce::Colour outlineVariant         { 0xFF49454F };

    // Primary
    inline const juce::Colour primary                { 0xFFD0BCFF };
    inline const juce::Colour onPrimary              { 0xFF381E72 };
    inline const juce::Colour primaryContainer       { 0xFF4F378B };
    inline const juce::Colour onPrimaryContainer     { 0xFFEADDFF };

    // Secondary
    inline const juce::Colour secondaryContainer     { 0xFF4A4458 };
    inline const juce::Colour onSecondaryContainer   { 0xFFE8DEF8 };

    // Tertiary (accents / highlights)
    inline const juce::Colour tertiary                 { 0xFFEFB8C8 };
    inline const juce::Colour tertiaryContainer      { 0xFF633B48 };
    inline const juce::Colour onTertiaryContainer    { 0xFFFFD8E4 };

    // State overlays (approximate “state layers” on containers)
    inline juce::Colour stateHover (juce::Colour base)   { return base.interpolatedWith (juce::Colours::white, 0.08f); }
    inline juce::Colour statePressed (juce::Colour base) { return base.interpolatedWith (juce::Colours::white, 0.12f); }

    // Shapes (corner radius, dp → px: 1:1 at 100% scale)
    inline constexpr float cornerNone   = 0.0f;
    inline constexpr float cornerExtraSmall = 4.0f;
    inline constexpr float cornerSmall  = 8.0f;
    inline constexpr float cornerMedium = 12.0f;
    inline constexpr float cornerLarge  = 16.0f;
    inline constexpr float cornerExtraLarge = 28.0f;

    inline void drawShadow (juce::Graphics& g, juce::Rectangle<float> bounds,
                            float cornerRadius, int elevationLevel)
    {
        if (elevationLevel <= 0 || cornerRadius <= 0.0f)
            return;

        const float yOff = 1.0f + 0.6f * (float) elevationLevel;
        const float blur = 2.5f + 1.2f * (float) elevationLevel;
        const float alpha = juce::jlimit (0.04f, 0.22f, 0.06f + 0.035f * (float) elevationLevel);

        juce::Path p;
        p.addRoundedRectangle (bounds.translated (0.0f, yOff), cornerRadius);
        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.fillPath (p); // cheap “drop shadow”; good enough for plugin UI
        juce::ignoreUnused (blur);
    }

    inline void fillSurface (juce::Graphics& g, juce::Rectangle<float> bounds,
                             juce::Colour fill, float cornerRadius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);
    }
}

// Legacy alias tokens (keeps existing call sites readable)
namespace AnimalColors
{
    inline const juce::Colour& Background   = AnimalM3::surface;
    inline const juce::Colour& Panel          = AnimalM3::surfaceContainer;
    inline const juce::Colour& Border         = AnimalM3::outlineVariant;
    inline const juce::Colour& Accent         = AnimalM3::primary;
    inline const juce::Colour& AccentBright   = AnimalM3::onPrimaryContainer;
    inline const juce::Colour& TextPrimary    = AnimalM3::onSurface;
    inline const juce::Colour& TextDim        = AnimalM3::onSurfaceVariant;
    inline const juce::Colour& ActiveStep     = AnimalM3::primary;
    inline const juce::Colour& PlayheadCol    = AnimalM3::primary;

    // Per-drum colours (seed hues, slightly tuned for dark surfaces)
    inline const juce::Colour DrumColors[9] = {
        juce::Colour (0xFFFF8A80),
        juce::Colour (0xFFFFD54F),
        juce::Colour (0xFF80D8FF),
        juce::Colour (0xFF80CBC4),
        juce::Colour (0xFFFFB74D),
        juce::Colour (0xFFFF8A65),
        juce::Colour (0xFFFFAB91),
        juce::Colour (0xFFB39DDB),
        juce::Colour (0xFF90CAF9),
    };
}

// ─────────────────────────────────────────────────────────────────────────────
class AnimalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnimalLookAndFeel()
    {
        using namespace AnimalM3;

        setColour (juce::ResizableWindow::backgroundColourId, surface);
        setColour (juce::Slider::thumbColourId, primary);
        setColour (juce::Slider::trackColourId, outlineVariant);
        setColour (juce::Slider::backgroundColourId, surfaceContainerHigh);
        setColour (juce::Slider::rotarySliderFillColourId, primary);
        setColour (juce::Slider::rotarySliderOutlineColourId, outlineVariant);

        setColour (juce::Label::textColourId, onSurface);

        setColour (juce::TextButton::buttonColourId, surfaceContainerHigh);
        setColour (juce::TextButton::buttonOnColourId, secondaryContainer);
        setColour (juce::TextButton::textColourOffId, onSurfaceVariant);
        setColour (juce::TextButton::textColourOnId, onSecondaryContainer);

        setColour (juce::ComboBox::backgroundColourId, surfaceContainerHigh);
        setColour (juce::ComboBox::textColourId, onSurface);
        setColour (juce::ComboBox::outlineColourId, outline);

        setColour (juce::PopupMenu::backgroundColourId, surfaceContainerHigh);
        setColour (juce::PopupMenu::textColourId, onSurface);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, primaryContainer);
        setColour (juce::PopupMenu::highlightedTextColourId, onPrimaryContainer);

        setColour (juce::ToggleButton::textColourId, onSurfaceVariant);
        setColour (juce::ToggleButton::tickColourId, primary);
        setColour (juce::ToggleButton::tickDisabledColourId, outlineVariant);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions().withHeight (12.0f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        juce::ignoreUnused (buttonHeight);
        return juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Semibold"));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        using namespace AnimalM3;

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        // Track
        juce::Path track;
        track.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outlineVariant.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (primary);
        g.strokePath (valueArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob “container”
        float knobR = radius - 10.0f;
        auto knobBounds = juce::Rectangle<float> (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
        drawShadow (g, knobBounds, cornerExtraLarge, 1);
        fillSurface (g, knobBounds, surfaceContainerHighest, cornerExtraLarge);
        g.setColour (outline.withAlpha (0.35f));
        g.drawRoundedRectangle (knobBounds, cornerExtraLarge, 1.0f);

        // Handle
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
        using namespace AnimalM3;

        auto track = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);

        if (style == juce::Slider::LinearHorizontal)
        {
            auto r = track.withSize (track.getWidth(), 4.0f).withCentre (track.getCentre());
            fillSurface (g, r, surfaceContainerHighest, cornerExtraSmall);
            g.setColour (outline.withAlpha (0.35f));
            g.drawRoundedRectangle (r, cornerExtraSmall, 1.0f);

            float xFill = juce::jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
            auto fill = r.withRight (xFill);
            fillSurface (g, fill, primaryContainer, cornerExtraSmall);

            float thumbX = juce::jmap (sliderPos, minSliderPos, maxSliderPos, r.getX(), r.getRight());
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
        using namespace AnimalM3;

        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        fillSurface (g, bounds.reduced (0.5f), surfaceContainerHigh, cornerSmall);
        g.setColour (outline.withAlpha (isButtonDown ? 0.9f : 0.55f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSmall, 1.0f);

        juce::Rectangle<int> arrowZone (buttonX, buttonY, buttonW, buttonH);
        juce::Path p;
        p.addTriangle ((float) arrowZone.getCentreX() - 4.0f, (float) arrowZone.getCentreY() - 2.0f,
                       (float) arrowZone.getCentreX() + 4.0f, (float) arrowZone.getCentreY() - 2.0f,
                       (float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
        g.setColour (onSurfaceVariant);
        g.fillPath (p);

        juce::ignoreUnused (box);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        using namespace AnimalM3;

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = cornerSmall;

        if (auto* tb = dynamic_cast<juce::TextButton*> (&button))
        {
            const auto text = tb->getButtonText();

            auto isPrimaryFilled = text == "GEN" || text == "FILL" || text == "PERF";
            auto isToggleStyle   = tb->getClickingTogglesState();

            juce::Colour fill = surfaceContainerHigh;
            juce::Colour stroke = outline.withAlpha (0.35f);
            juce::Colour content = onSurface;

            if (isPrimaryFilled)
            {
                fill = primaryContainer;
                content = onPrimaryContainer;
                stroke = juce::Colours::transparentBlack;
            }
            else if (isToggleStyle && tb->getToggleState())
            {
                fill = secondaryContainer;
                content = onSecondaryContainer;
                stroke = juce::Colours::transparentBlack;
            }
            else if (isToggleStyle && ! tb->getToggleState())
            {
                fill = surfaceContainer;
                content = onSurfaceVariant;
                stroke = outline.withAlpha (0.55f);
            }

            if (shouldDrawButtonAsDown)   fill = statePressed (fill);
            else if (shouldDrawButtonAsHighlighted) fill = stateHover (fill);

            if (isPrimaryFilled || (isToggleStyle && tb->getToggleState()))
                drawShadow (g, bounds, rad, 1);

            fillSurface (g, bounds, fill, rad);
            g.setColour (stroke);
            g.drawRoundedRectangle (bounds, rad, 1.0f);

            g.setColour (content);
            g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Semibold")));
            g.drawFittedText (text, button.getLocalBounds(), juce::Justification::centred, 1);
            return;
        }

        juce::LookAndFeel_V4::drawButtonBackground (g, button, button.findColour (juce::TextButton::buttonColourId),
                                                    shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*highlighted*/, bool /*down*/) override
    {
        // Text is drawn in drawButtonBackground for TextButtons (M3 filled / tonal).
        juce::ignoreUnused (g, button);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        using namespace AnimalM3;

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = button.getToggleState();

        juce::Colour fill = surfaceContainerHigh;
        if (button.getButtonText() == "S" && on)
            fill = juce::Colour (0xff143d28);
        else if (button.getButtonText() == "M" && on)
            fill = surfaceBright;

        if (shouldDrawButtonAsDown) fill = statePressed (fill);
        else if (shouldDrawButtonAsHighlighted) fill = stateHover (fill);

        drawShadow (g, bounds, cornerExtraSmall, on ? 1 : 0);
        fillSurface (g, bounds, fill, cornerExtraSmall);
        g.setColour (outline.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds, cornerExtraSmall, 1.0f);

        g.setColour (button.getButtonText() == "S" && on
                          ? juce::Colour (0xff7dffb3)
                          : (on ? onSurface : onSurfaceVariant));
        g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f).withStyle ("Semibold")));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }
};
