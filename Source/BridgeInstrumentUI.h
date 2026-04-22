#pragma once

#include <JuceHeader.h>
#include "BridgeLookAndFeel.h"

/** Circular power toggle: accent fill when on, gray when off. */
class PagePowerButton : public juce::ToggleButton
{
public:
    explicit PagePowerButton (juce::Colour accentColour)
        : accent (accentColour)
    {
        setClickingTogglesState (true);
        setTooltip ("Engine on / off");
    }

    void setAccent (juce::Colour c)
    {
        accent = c;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        juce::ignoreUnused (over, down);
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();

        const auto offFill = bridge::hig::quaternaryFill;
        const auto onFill  = accent.withAlpha (0.85f);
        g.setColour (on ? onFill : offFill);
        g.fillEllipse (b);

        g.setColour (juce::Colours::white.withAlpha (on ? 0.95f : 0.45f));
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        const float r  = juce::jmin (b.getWidth(), b.getHeight()) * 0.22f;
        g.drawVerticalLine (juce::roundToInt (cx), juce::roundToInt (cy - r * 0.2f), juce::roundToInt (cy + r * 1.1f));
        juce::Path arc;
        arc.addCentredArc (cx, cy - r * 0.15f, r * 1.35f, r * 1.35f, 0.0f, 1.25f * juce::MathConstants<float>::pi,
                           1.25f * juce::MathConstants<float>::pi, true);
        g.strokePath (arc, juce::PathStrokeType (1.4f));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (b.reduced (0.5f), 1.0f);
    }

private:
    juce::Colour accent;
};
