#pragma once

#include <cmath>
#include <utility>
#include <JuceHeader.h>
#include "bass/BassEngine.h"
#include "bass/BassStylePresets.h"
#include "piano/PianoEngine.h"
#include "piano/PianoStylePresets.h"
#include "guitar/GuitarEngine.h"
#include "guitar/GuitarStylePresets.h"

// Shared melodic grid logic: pitch window from the committed pattern (viewport + zoom)
// and square cell layout.

namespace bridge
{
/** After zoom changes body dimensions, keep the same relative scroll position (centre-stable). */
inline void melodicViewportPreserveCentreAfterBodyResize (juce::Viewport& vp,
                                                          int oldViewX,
                                                          int oldViewY,
                                                          int oldBodyW,
                                                          int oldBodyH)
{
    auto* body = vp.getViewedComponent();
    if (body == nullptr || oldBodyW < 1 || oldBodyH < 1)
        return;

    const int vpW = juce::jmax (1, vp.getWidth());
    const int vpH = juce::jmax (1, vp.getHeight());
    const float relX = ((float) oldViewX + 0.5f * (float) vpW) / (float) oldBodyW;
    const float relY = ((float) oldViewY + 0.5f * (float) vpH) / (float) oldBodyH;
    const int newBodyW = body->getWidth();
    const int newBodyH = body->getHeight();
    int newX = (int) std::lround (relX * (float) newBodyW - 0.5f * (float) vpW);
    int newY = (int) std::lround (relY * (float) newBodyH - 0.5f * (float) vpH);
    const int maxX = juce::jmax (0, newBodyW - vpW);
    const int maxY = juce::jmax (0, newBodyH - vpH);
    vp.setViewPosition (juce::jlimit (0, maxX, newX), juce::jlimit (0, maxY, newY));
}


/** Legacy default row count when callers still assume a single-octave window. */
inline constexpr int kMelodicOctaveRows = 12;
inline constexpr int kMelodicPitchPadSemis = 0;
/** Fixed piano-roll pitch span (C0–C6); viewport scrolls and zoom scales row height. */
inline constexpr int kMelodicMinMidi = 12;
inline constexpr int kMelodicMaxMidi = 84;

inline void computeMelodicPitchWindowFromCommittedPattern (const PianoEngine&, int& minMidi, int& maxMidi)
{
    minMidi = kMelodicMinMidi;
    maxMidi = kMelodicMaxMidi;
}

inline void computeMelodicPitchWindowFromCommittedPattern (const BassEngine&, int& minMidi, int& maxMidi)
{
    minMidi = kMelodicMinMidi;
    maxMidi = kMelodicMaxMidi;
}

inline void computeMelodicPitchWindowFromCommittedPattern (const GuitarEngine&, int& minMidi, int& maxMidi)
{
    minMidi = kMelodicMinMidi;
    maxMidi = kMelodicMaxMidi;
}

inline std::pair<int, int> getPatternMidiExtent (const PianoEngine& engine)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPattern();
    const int plen = engine.getPatternLen();
    for (int s = 0; s < plen; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }
    return { pMin, pMax };
}

inline std::pair<int, int> getPatternMidiExtent (const BassEngine& engine)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPattern();
    const int plen = engine.getPatternLen();
    for (int s = 0; s < plen; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }
    return { pMin, pMax };
}

inline std::pair<int, int> getPatternMidiExtent (const GuitarEngine& engine)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPattern();
    const int plen = engine.getPatternLen();
    for (int s = 0; s < plen; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }
    return { pMin, pMax };
}
/** Left column: piano roll (melodic) or drum lane labels + M/S (drums). */
inline constexpr int kMelodicKeyStripWidth = 64;
/** Loop + step division strip under the instrument control bar. */
inline constexpr int kLoopRangeStripHeightPx = 18;

inline void setOneOctaveMelodicRange (const BassEngine& engine, int& minMidi, int& maxMidi)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < BassPreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }

    const int root = engine.degreeToMidiNote (0, -1);

    if (pMin > pMax)
        minMidi = juce::jlimit (0, 115, (root / 12) * 12);
    else if (pMax - pMin > 11)
        minMidi = juce::jlimit (0, 115, pMax - 11);
    else
        minMidi = juce::jlimit (0, 115, pMin);

    maxMidi = minMidi + 11;
}

inline void setOneOctaveMelodicRange (const PianoEngine& engine, int& minMidi, int& maxMidi)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < PianoPreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }

    const int root = engine.degreeToMidiNote (0, -1);

    if (pMin > pMax)
        minMidi = juce::jlimit (0, 115, (root / 12) * 12);
    else if (pMax - pMin > 11)
        minMidi = juce::jlimit (0, 115, pMax - 11);
    else
        minMidi = juce::jlimit (0, 115, pMin);

    maxMidi = minMidi + 11;
}

inline void setOneOctaveMelodicRange (const GuitarEngine& engine, int& minMidi, int& maxMidi)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < GuitarPreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        pMin = juce::jmin (pMin, h.midiNote);
        pMax = juce::jmax (pMax, h.midiNote);
    }

    const int root = engine.degreeToMidiNote (0, -1);

    if (pMin > pMax)
        minMidi = juce::jlimit (0, 115, (root / 12) * 12);
    else if (pMax - pMin > 11)
        minMidi = juce::jlimit (0, 115, pMax - 11);
    else
        minMidi = juce::jlimit (0, 115, pMin);

    maxMidi = minMidi + 11;
}

/** Square cells; grid centred in the inner body area. */
inline void computeSquareMelodicGrid (float innerX, float innerW, float gridTop, float hBody,
                                      int numSteps, int nRows,
                                      float& originX, float& originY, float& cellSize)
{
    const float cw = innerW / (float) juce::jmax (1, numSteps);
    const float ch = hBody / (float) juce::jmax (1, nRows);
    cellSize = juce::jmin (cw, ch);
    originX = innerX + (innerW - cellSize * (float) numSteps) * 0.5f;
    originY = gridTop + (hBody - cellSize * (float) nRows) * 0.5f;
}

} // namespace bridge
