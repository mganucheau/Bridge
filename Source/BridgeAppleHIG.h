#pragma once

#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// Apple Human Interface Guidelines — dark mode semantic colours & layout tokens
// (approximations for custom-drawn JUCE UI; see Apple HIG “Color” / “Layout”.)
// https://developer.apple.com/design/human-interface-guidelines
// ─────────────────────────────────────────────────────────────────────────────
namespace bridge::hig
{
    // MARK: Surfaces (dark appearance, elevated / grouped hierarchy)
    inline const juce::Colour systemBackground           { 0xFF000000 };
    inline const juce::Colour secondaryGroupedBackground { 0xFF1C1C1E };
    inline const juce::Colour tertiaryGroupedBackground  { 0xFF2C2C2E };
    inline const juce::Colour quaternaryFill             { 0xFF3A3A3C };
    inline const juce::Colour separatorOpaque             { 0xFF38383A };

    // MARK: Labels
    inline const juce::Colour label            { 0xFFFFFFFF };
    inline const juce::Colour secondaryLabel   { 0xFFAEAEB2 };
    inline const juce::Colour tertiaryLabel    { 0xFF636366 };
    inline const juce::Colour quaternaryLabel  { 0xFF48484A };

    // System accents (dark)
    inline const juce::Colour systemBlue   { 0xFF0A84FF };
    inline const juce::Colour systemGreen  { 0xFF32D74B };
    inline const juce::Colour systemOrange { 0xFFFF9F0A };
    inline const juce::Colour systemYellow { 0xFFFFD60A };

    // MARK: Geometry — corner radii (pt-aligned at 1×; matches common HIG control shapes)
    inline constexpr float cornerInlineControl = 6.0f;   // small toggles, inline fields
    inline constexpr float cornerStandard      = 10.0f;  // buttons, combo boxes, text fields
    inline constexpr float cornerLargeSurface  = 12.0f;  // cards, grouped regions

    inline juce::Colour accentFillSubdued (juce::Colour accent)
    {
        return tertiaryGroupedBackground.interpolatedWith (accent, 0.42f);
    }

    inline juce::Colour stateHover (juce::Colour base)
    {
        return base.interpolatedWith (juce::Colours::white, 0.08f);
    }

    inline juce::Colour statePressed (juce::Colour base)
    {
        return base.interpolatedWith (juce::Colours::white, 0.12f);
    }

    inline void drawElevatedShadow (juce::Graphics& g, juce::Rectangle<float> bounds,
                                    float cornerRadius, int elevationLevel)
    {
        if (elevationLevel <= 0 || cornerRadius <= 0.0f)
            return;

        const float yOff = 1.0f + 0.55f * (float) elevationLevel;
        const float alpha = juce::jlimit (0.04f, 0.18f, 0.05f + 0.03f * (float) elevationLevel);

        juce::Path p;
        p.addRoundedRectangle (bounds.translated (0.0f, yOff), cornerRadius);
        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.fillPath (p);
    }

    inline void fillRounded (juce::Graphics& g, juce::Rectangle<float> bounds,
                             juce::Colour fill, float cornerRadius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);
    }

    /** Prefer this for UI text so macOS/iOS use San Francisco via the system UI font. */
    inline juce::Font uiFont (float sizePt, const juce::String& style = {})
    {
        juce::FontOptions opt = juce::FontOptions().withHeight (sizePt);
        if (style.isNotEmpty())
            opt = opt.withStyle (style);
#if JUCE_MAC || JUCE_IOS
        opt = opt.withName (".AppleSystemUIFont");
#endif
        return juce::Font (opt);
    }
}
