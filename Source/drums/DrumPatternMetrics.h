#pragma once

#include "DrumEngine.h"

namespace bridge::drums
{

/** Read-only stats for CI / QA on generated grids (human-feel invariants). */
struct DrumPatternStats
{
    int   totalActive       = 0;
    int   maxStepPolyphony  = 0;
    double activeRate        = 0.0; // totalActive / (patternLen * NUM_DRUMS)
    int   patternLen        = 0;
};

/** Count active hits; max concurrent voices on any 16th step. */
inline DrumPatternStats measurePattern (const DrumPattern& p, int patternLen) noexcept
{
    DrumPatternStats s;
    s.patternLen = juce::jlimit (1, NUM_STEPS, patternLen);
    const int cells = s.patternLen * NUM_DRUMS;
    for (int step = 0; step < s.patternLen; ++step)
    {
        int n = 0;
        for (int d = 0; d < NUM_DRUMS; ++d)
            if (p[(size_t) step][(size_t) d].active)
            {
                ++n;
                ++s.totalActive;
            }
        s.maxStepPolyphony = juce::jmax (s.maxStepPolyphony, n);
    }
    if (cells > 0)
        s.activeRate = (double) s.totalActive / (double) cells;
    return s;
}

} // namespace bridge::drums
