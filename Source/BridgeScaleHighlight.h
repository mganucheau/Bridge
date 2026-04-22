#pragma once

#include "bass/BassStylePresets.h"
#include <JuceHeader.h>

namespace bridge
{

/** Pitch-class in scale (0..11) using main preset scale tables (shared across engines). */
inline bool pitchClassInPresetScale (int scaleIndex, int rootPc, int pitchClass) noexcept
{
    scaleIndex = juce::jlimit (0, BassPreset::NUM_SCALES - 1, scaleIndex);
    rootPc = (rootPc % 12 + 12) % 12;
    pitchClass = (pitchClass % 12 + 12) % 12;

    const int n = BassPreset::SCALE_TONE_COUNT[scaleIndex];
    for (int i = 0; i < n; ++i)
    {
        const int off = BassPreset::SCALE_INTERVALS[scaleIndex][i];
        if (off < 0)
            break;
        const int pc = (rootPc + off) % 12;
        if (pc == pitchClass)
            return true;
    }
    return false;
}

} // namespace bridge
