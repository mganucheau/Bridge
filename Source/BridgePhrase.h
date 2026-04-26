#pragma once

#include <JuceHeader.h>

namespace bridge::phrase
{
// One bar is always a 16th-note grid with 16 steps.
inline constexpr int kStepsPerBar   = 16;
inline constexpr int kMaxPhraseBars = 16;
inline constexpr int kMaxSteps      = kStepsPerBar * kMaxPhraseBars; // 256

/** Main header "phrase length" dropdown: idx 0=2 bars … 3=16 bars. */
inline int phraseBarsFromChoiceIndex (int idx) noexcept
{
    idx = juce::jlimit (0, 3, idx);
    switch (idx)
    {
        case 0: return 2;
        case 1: return 4;
        case 2: return 8;
        default: return 16;
    }
}

/** Jam period dropdown (4/8/16 bars only) — 3 choices, indices 0..2. */
inline int jamPeriodBarsFromChoiceIndex (int idx) noexcept
{
    idx = juce::jlimit (0, 2, idx);
    return idx == 0 ? 4 : (idx == 1 ? 8 : 16);
}

inline int phraseStepsForBars (int bars) noexcept
{
    return juce::jlimit (kStepsPerBar, kMaxSteps, bars * kStepsPerBar);
}

inline int clampStep1BasedToPhrase (int step1, int phraseSteps) noexcept
{
    return juce::jlimit (1, juce::jmax (1, phraseSteps), step1);
}
} // namespace bridge::phrase
