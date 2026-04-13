#include "BridgeLoopRangeStrip.h"
#include <cmath>

namespace
{
static juce::Rectangle<float> trackBounds (juce::Rectangle<float> b)
{
    return b.reduced (2.0f, 5.0f);
}
}

BridgeLoopRangeStrip::BridgeLoopRangeStrip (juce::AudioProcessorValueTreeState& ap,
                                            juce::Colour accentColour,
                                            int steps)
    : apvts (ap), accent (accentColour), numSteps (juce::jmax (2, steps))
{
    setOpaque (false);
}

int BridgeLoopRangeStrip::readIntParam (juce::AudioProcessorValueTreeState& ap,
                                        const juce::String& id, int fallback)
{
    if (auto* p = ap.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            return ip->get();
    return fallback;
}

void BridgeLoopRangeStrip::writeIntParam (juce::AudioProcessorValueTreeState& ap,
                                          const juce::String& id, int v, int maxStep)
{
    v = juce::jlimit (1, maxStep, v);
    if (auto* p = ap.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) v));
}

int BridgeLoopRangeStrip::xToStep (float x) const
{
    auto b = trackBounds (getLocalBounds().toFloat());
    float t = (x - b.getX()) / juce::jmax (1.0f, b.getWidth());
    int s = 1 + (int) (t * (float) numSteps);
    return juce::jlimit (1, numSteps, s);
}

void BridgeLoopRangeStrip::syncFromMouse (const juce::MouseEvent& e, bool isDrag)
{
    juce::ignoreUnused (isDrag);
    int step = xToStep (e.position.x);
    int ls = readIntParam (apvts, "loopStart", 1);
    int le = readIntParam (apvts, "loopEnd", numSteps);
    ls = juce::jlimit (1, numSteps, ls);
    le = juce::jlimit (1, numSteps, le);
    if (le < ls)
    {
        const int t = ls;
        ls = le;
        le = t;
    }

    if (dragMode == 1)
    {
        ls = step;
        if (ls > le)
            le = ls;
    }
    else if (dragMode == 2)
    {
        le = step;
        if (le < ls)
            ls = le;
    }

    writeIntParam (apvts, "loopStart", ls, numSteps);
    writeIntParam (apvts, "loopEnd", le, numSteps);
    repaint();
}

void BridgeLoopRangeStrip::mouseDown (const juce::MouseEvent& e)
{
    auto b = trackBounds (getLocalBounds().toFloat());
    const float cell = b.getWidth() / (float) numSteps;
    int ls = readIntParam (apvts, "loopStart", 1);
    int le = readIntParam (apvts, "loopEnd", numSteps);
    ls = juce::jlimit (1, numSteps, ls);
    le = juce::jlimit (1, numSteps, le);

    const float sx = b.getX() + ((float) ls - 0.5f) * cell;
    const float ex = b.getX() + ((float) le - 0.5f) * cell;
    const float px = e.position.x;

    if (std::abs (px - sx) <= std::abs (px - ex))
        dragMode = 1;
    else
        dragMode = 2;

    syncFromMouse (e, false);
}

void BridgeLoopRangeStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode != 0)
        syncFromMouse (e, true);
}

void BridgeLoopRangeStrip::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    dragMode = 0;
}

void BridgeLoopRangeStrip::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    auto b    = trackBounds (full);

    g.setColour (juce::Colour (0xff14121a));
    g.fillRoundedRectangle (b, 5.0f);

    int ls = readIntParam (apvts, "loopStart", 1);
    int le = readIntParam (apvts, "loopEnd", numSteps);
    ls = juce::jlimit (1, numSteps, ls);
    le = juce::jlimit (1, numSteps, le);
    if (le < ls)
    {
        const int t = ls;
        ls = le;
        le = t;
    }

    const float cell = b.getWidth() / (float) numSteps;

    g.setColour (accent.withAlpha (0.28f));
    g.fillRect (b.getX() + (float) (ls - 1) * cell, b.getY(),
                (float) (le - ls + 1) * cell, b.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    for (int i = 1; i < numSteps; ++i)
    {
        float x = b.getX() + (float) i * cell;
        g.drawVerticalLine ((int) x, (int) b.getY(), (int) b.getBottom());
    }

    auto drawHandle = [&] (int step, bool start)
    {
        float cx = b.getX() + ((float) step - 0.5f) * cell;
        float cy = b.getCentreY();
        juce::Path tri;
        const float h = juce::jmin (b.getHeight() * 0.42f, 12.0f);
        if (start)
        {
            tri.addTriangle (cx - 5.0f, cy,
                             cx + 6.0f, cy - h * 0.55f,
                             cx + 6.0f, cy + h * 0.55f);
        }
        else
        {
            tri.addTriangle (cx + 5.0f, cy,
                             cx - 6.0f, cy - h * 0.55f,
                             cx - 6.0f, cy + h * 0.55f);
        }
        g.setColour (accent.brighter (0.15f));
        g.fillPath (tri);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.strokePath (tri, juce::PathStrokeType (0.8f));
    };

    drawHandle (ls, true);
    drawHandle (le, false);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
}
