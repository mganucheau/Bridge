#pragma once

#include <JuceHeader.h>
#include <cmath>

/** Filled paths approximating Lucide-style Repeat and RefreshCw icons for ShapeButton. */
namespace bridge::icons
{

inline juce::Path lucideRefreshCwFilled (juce::Rectangle<float> b)
{
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float s = juce::jmin (b.getWidth(), b.getHeight());
    const float r = s * 0.38f;

    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.0f,
                       juce::MathConstants<float>::halfPi * 0.35f,
                       juce::MathConstants<float>::twoPi * 0.88f,
                       true);

    juce::Path stroked;
    juce::PathStrokeType pst (s * 0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    pst.createStrokedPath (stroked, arc);

    const float ang = juce::MathConstants<float>::twoPi * 0.88f - 0.1f;
    const float ex = cx + r * std::cos (ang);
    const float ey = cy + r * std::sin (ang);
    juce::Path ar;
    ar.addTriangle (ex, ey, ex - s * 0.13f, ey - s * 0.07f, ex - s * 0.04f, ey + s * 0.11f);
    stroked.addPath (ar);
    return stroked;
}

/** Two opposing arcs + corner arrows = “playback loop / repeat range”. */
inline juce::Path lucideRepeatFilled (juce::Rectangle<float> b)
{
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float s = juce::jmin (b.getWidth(), b.getHeight());
    const float r = s * 0.30f;
    const float oy = s * 0.10f;
    const float sw = juce::jmax (1.2f, s * 0.11f);

    juce::PathStrokeType pst (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path out;

    {
        juce::Path a;
        a.addCentredArc (cx, cy - oy, r, r, 0.0f,
                         juce::MathConstants<float>::pi * 1.05f,
                         juce::MathConstants<float>::pi * 2.05f,
                         true);
        juce::Path t;
        pst.createStrokedPath (t, a);
        out.addPath (t);
        const float ax = cx + r * 0.85f;
        const float ay = cy - oy - r * 0.35f;
        juce::Path tip;
        tip.addTriangle (ax, ay, ax - s * 0.16f, ay + s * 0.12f, ax + s * 0.05f, ay + s * 0.14f);
        out.addPath (tip);
    }
    {
        juce::Path a;
        a.addCentredArc (cx, cy + oy, r, r, 0.0f,
                         juce::MathConstants<float>::pi * -0.05f,
                         juce::MathConstants<float>::pi * 0.95f,
                         true);
        juce::Path t;
        pst.createStrokedPath (t, a);
        out.addPath (t);
        const float ax = cx - r * 0.85f;
        const float ay = cy + oy + r * 0.35f;
        juce::Path tip;
        tip.addTriangle (ax, ay, ax + s * 0.16f, ay - s * 0.12f, ax - s * 0.05f, ay - s * 0.14f);
        out.addPath (tip);
    }

    return out;
}

/** Closed padlock — span / width lock (not transport refresh). */
inline juce::Path lucideLockFilled (juce::Rectangle<float> b)
{
    const float s = juce::jmin (b.getWidth(), b.getHeight());
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float bodyW = s * 0.42f;
    const float bodyH = s * 0.34f;
    const float shackleR = s * 0.14f;
    const float topY = cy - bodyH * 0.5f - shackleR * 1.35f;

    juce::Path p;
    p.addRoundedRectangle (cx - bodyW * 0.5f, cy - bodyH * 0.35f, bodyW, bodyH, s * 0.06f, s * 0.06f);

    juce::Path arch;
    arch.addCentredArc (cx, topY + shackleR, shackleR, shackleR, 0.0f,
                        juce::MathConstants<float>::pi,
                        juce::MathConstants<float>::twoPi,
                        true);
    juce::PathStrokeType pst (s * 0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path stroked;
    pst.createStrokedPath (stroked, arch);
    p.addPath (stroked);

    return p;
}

} // namespace bridge::icons
