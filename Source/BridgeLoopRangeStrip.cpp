#include "BridgeLoopRangeStrip.h"
#include "BridgeLookAndFeel.h"
#include "BridgeAppleHIG.h"
#include <cmath>

namespace
{
static juce::Rectangle<float> trackBounds (juce::Rectangle<float> b, int labelGutterLeft)
{
    auto r = b.reduced (2.0f, 3.0f);
    if (labelGutterLeft > 0)
        r.removeFromLeft ((float) labelGutterLeft);
    return r;
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

void BridgeLoopRangeStrip::setBarRepeats (int repeats)
{
    repeats = juce::jlimit (1, 64, repeats);
    if (repeats == barRepeats)
        return;
    barRepeats = repeats;
    repaint();
}

void BridgeLoopRangeStrip::setNumSteps (int steps)
{
    steps = juce::jlimit (2, 256, steps);
    if (steps == numSteps)
        return;
    numSteps = steps;
    repaint();
}

int BridgeLoopRangeStrip::xToStep (float x) const
{
    auto b = trackBounds (getLocalBounds().toFloat(), stepLabelGutterLeft);
    const int totalCells = numSteps * juce::jmax (1, barRepeats);
    float t = (x - b.getX()) / juce::jmax (1.0f, b.getWidth());
    int cell = (int) (t * (float) totalCells);
    cell = juce::jlimit (0, totalCells - 1, cell);
    // Map any visible bar back to bar 1 — the underlying loop selection lives in 1..numSteps.
    int s = 1 + (cell % numSteps);
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
    auto b = trackBounds (getLocalBounds().toFloat(), stepLabelGutterLeft);
    const int repeats = juce::jmax (1, barRepeats);
    const float cell = b.getWidth() / (float) (numSteps * repeats);
    int ls = readIntParam (apvts, "loopStart", 1);
    int le = readIntParam (apvts, "loopEnd", numSteps);
    ls = juce::jlimit (1, numSteps, ls);
    le = juce::jlimit (1, numSteps, le);

    // Pick the nearest handle by considering every bar repeat — feels right when the user clicks
    // any bar in the tiled phrase view.
    const float px = e.position.x;
    float bestStartDist = std::numeric_limits<float>::max();
    float bestEndDist   = std::numeric_limits<float>::max();
    for (int bar = 0; bar < repeats; ++bar)
    {
        const float sx = b.getX() + ((float) (bar * numSteps + ls) - 0.5f) * cell;
        const float ex = b.getX() + ((float) (bar * numSteps + le) - 0.5f) * cell;
        bestStartDist = juce::jmin (bestStartDist, std::abs (px - sx));
        bestEndDist   = juce::jmin (bestEndDist,   std::abs (px - ex));
    }

    dragMode = bestStartDist <= bestEndDist ? 1 : 2;

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
    auto b    = trackBounds (full, stepLabelGutterLeft);

    g.setColour (bridge::colors::background());
    g.fillRect (full);

    if (stepLabelGutterLeft > 0)
    {
        auto gutter = full.withWidth ((float) stepLabelGutterLeft);
        g.setColour (bridge::hig::tertiaryGroupedBackground);
        g.fillRect (gutter);
    }

    g.setColour (bridge::colors::background());
    g.fillRect (b);

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

    const int   repeats   = juce::jmax (1, barRepeats);
    const int   totalCells = numSteps * repeats;
    const float cell      = b.getWidth() / (float) totalCells;

    // Loop fill — tiled across each bar of the phrase.
    for (int bar = 0; bar < repeats; ++bar)
    {
        const float x0 = b.getX() + (float) (bar * numSteps + ls - 1) * cell;
        const float w  = (float) (le - ls + 1) * cell;
        g.setColour (accent.withAlpha (bar == 0 ? 0.52f : 0.34f));
        g.fillRect (x0, b.getY(), w, b.getHeight());
        g.setColour (accent.brighter (0.12f).withAlpha (bar == 0 ? 0.55f : 0.35f));
        g.drawRect (x0, b.getY(), w, b.getHeight(), 1.0f);
    }

    // Step grid lines — keep heavy bar boundaries even when phrase > 1 bar.
    for (int i = 1; i < totalCells; ++i)
    {
        const bool barBoundary = (i % numSteps) == 0;
        g.setColour (juce::Colours::white.withAlpha (barBoundary ? 0.32f : 0.10f));
        const float x = b.getX() + (float) i * cell;
        g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
    }

    auto drawHandle = [&] (int step, bool start, float alpha)
    {
        for (int bar = 0; bar < repeats; ++bar)
        {
            float cx = b.getX() + ((float) (bar * numSteps + step) - 0.5f) * cell;
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
            const float a = bar == 0 ? alpha : alpha * 0.55f;
            g.setColour (accent.brighter (0.15f).withAlpha (a));
            g.fillPath (tri);
            g.setColour (juce::Colours::white.withAlpha (0.5f * a));
            g.strokePath (tri, juce::PathStrokeType (0.8f));
        }
    };

    drawHandle (ls, true,  1.0f);
    drawHandle (le, false, 1.0f);

    // Step number labels — only draw when there is room (otherwise the labels overlap).
    const bool drawStepNumbers = cell >= 12.0f;
    if (drawStepNumbers)
    {
        g.setFont (bridge::hig::uiFont (10.5f, "Semibold"));
        for (int i = 0; i < totalCells; ++i)
        {
            float cx = b.getX() + (float) i * cell;
            auto rc = juce::Rectangle<float> (cx, b.getY(), cell, b.getHeight());
            const int label = 1 + (i % numSteps);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawText (juce::String (label), rc.translated (0.5f, 0.5f), juce::Justification::centred, false);
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.drawText (juce::String (label), rc, juce::Justification::centred, false);
        }
    }
    else if (repeats > 1)
    {
        // When the per-step label is too small, surface bar numbers (1..N) so the user can still
        // orient themselves inside the phrase.
        g.setFont (bridge::hig::uiFont (10.5f, "Semibold"));
        const float barW = (float) numSteps * cell;
        for (int bar = 0; bar < repeats; ++bar)
        {
            auto rc = juce::Rectangle<float> (b.getX() + (float) bar * barW, b.getY(), barW, b.getHeight());
            const juce::String s ("Bar " + juce::String (bar + 1));
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawText (s, rc.translated (0.5f, 0.5f), juce::Justification::centred, false);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (s, rc, juce::Justification::centred, false);
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRect (b, 1.0f);
}
