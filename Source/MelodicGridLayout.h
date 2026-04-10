#pragma once

#include <JuceHeader.h>
#include "bootsy/BassEngine.h"
#include "bootsy/BootsyStylePresets.h"
#include "stevie/PianoEngine.h"
#include "stevie/StevieStylePresets.h"
#include "paul/GuitarEngine.h"
#include "paul/PaulStylePresets.h"

// Shared melodic grid logic: one octave (12 pitch rows) and square cell layout.

namespace bridge
{

inline constexpr int kMelodicOctaveRows = 12;

inline void setOneOctaveMelodicRange (const BassEngine& engine, int& minMidi, int& maxMidi)
{
    int pMin = 127, pMax = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < BootsyPreset::NUM_STEPS; ++s)
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
    for (int s = 0; s < SteviePreset::NUM_STEPS; ++s)
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
    for (int s = 0; s < PaulPreset::NUM_STEPS; ++s)
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
