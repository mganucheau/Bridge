#include "BridgeVelocityStrip.h"

BridgeVelocityStrip::BridgeVelocityStrip (int n, juce::Colour acc)
    : numSteps (juce::jlimit (1, 64, n)), accent (acc)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (24);
}

void BridgeVelocityStrip::setStepRange (int from0, int to0)
{
    rangeFrom = juce::jmax (0, from0);
    rangeTo   = juce::jlimit (rangeFrom, numSteps - 1, to0);
    repaint();
}

void BridgeVelocityStrip::timerCallback()
{
    if (! velocityAt)
        return;

    bool changed = false;
    for (int s = 0; s < numSteps; ++s)
    {
        const float v = juce::jlimit (0.f, 1.f, (float) velocityAt (s) / 127.0f);
        if (std::abs (snapshot[(size_t) s] - v) > 0.005f)
        {
            snapshot[(size_t) s] = v;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void BridgeVelocityStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (r.isEmpty()) return;

    // Background
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 3.0f);

    // Mid-line tick marker
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * 0.55f),
                          r.getX() + 2.f, r.getRight() - 2.f);

    if (numSteps <= 0) return;

    const float colW = r.getWidth() / (float) numSteps;
    const float gap  = juce::jmin (1.5f, colW * 0.18f);
    const float maxBarH = r.getHeight() - 2.f;

    for (int s = 0; s < numSteps; ++s)
    {
        const float v   = snapshot[(size_t) s];
        const bool inRange = (rangeTo < 0) || (s >= rangeFrom && s <= rangeTo);
        if (v <= 0.001f)
            continue;

        const float h  = juce::jmax (1.5f, v * maxBarH);
        juce::Rectangle<float> bar (r.getX() + s * colW + gap,
                                    r.getBottom() - 1.f - h,
                                    juce::jmax (1.0f, colW - 2.f * gap),
                                    h);
        const float a = inRange ? juce::jmap (v, 0.f, 1.f, 0.55f, 1.0f)
                                : 0.20f;
        g.setColour (accent.withAlpha (a));
        g.fillRoundedRectangle (bar, 1.5f);
    }
}
