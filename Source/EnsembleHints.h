#pragma once

#include <array>
#include <vector>
#include "drums/DrumEngine.h"

namespace bridge::ensemble
{

/** Single read of drum activity + kick grid for follower engines (rhythm bus / ML kick). */
struct LeaderSnapshotFromDrums
{
    std::array<float, 16> stepActivity {};
    std::vector<float>     kickHint16; // size 16, values 0 or 1 for bass ONNX hint
};

inline LeaderSnapshotFromDrums snapshotFromDrumEngine (const DrumEngine& drum)
{
    LeaderSnapshotFromDrums s;
    s.kickHint16.assign (16, 0.0f);
    const auto act = drum.getStepActivityGrid();
    const auto& pat = drum.getPattern();
    const int pl = juce::jmin (16, drum.getPatternLen());
    for (int i = 0; i < 16; ++i)
    {
        s.stepActivity[(size_t) i] = act[(size_t) i];
        if (i < pl)
            s.kickHint16[(size_t) i] = pat[(size_t) i][0].active ? 1.0f : 0.0f;
    }
    return s;
}

} // namespace bridge::ensemble
