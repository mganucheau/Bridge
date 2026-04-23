#pragma once

#include <array>
#include <cmath>
#include <cstddef>

// Named ML personality presets — keep vectors in sync with ml/training/personality_presets.py
namespace bridge::personality
{

struct PresetEntry
{
    const char* id;
    const char* displayName;
    std::array<float, 10> knobs;
};

inline constexpr PresetEntry kPresets[] = {
    { "driving_rock", "Driving rock",
      { 0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.6f, 0.3f, 0.8f, 0.7f, 0.2f } },
    { "jazz_trio", "Jazz trio",
      { 0.4f, 0.9f, 0.3f, 0.8f, 0.6f, 0.8f, 0.9f, 0.2f, 0.4f, 0.6f } },
    { "minimal_techno", "Minimal techno",
      { 0.95f, 0.2f, 0.5f, 0.3f, 0.05f, 0.2f, 0.2f, 0.9f, 0.3f, 0.1f } },
    { "singer_songwriter", "Singer-songwriter",
      { 0.55f, 0.75f, 0.6f, 0.65f, 0.25f, 0.85f, 0.35f, 0.75f, 0.45f, 0.55f } },
    { "funk_groove", "Funk groove",
      { 0.85f, 0.65f, 0.55f, 0.5f, 0.45f, 0.7f, 0.5f, 0.4f, 0.85f, 0.4f } },
    { "ambient", "Ambient",
      { 0.35f, 0.85f, 0.9f, 0.75f, 0.15f, 0.55f, 0.75f, 0.35f, 0.25f, 0.45f } },
};

inline constexpr size_t numNamedPresets = sizeof (kPresets) / sizeof (kPresets[0]);

/** APVTS AudioParameterChoice index: 0 = Custom, 1..numNamedPresets = presets in kPresets order. */
inline int indexMatchingKnobs (const std::array<float, 10>& knobs, float epsilon = 0.005f)
{
    for (size_t pi = 0; pi < numNamedPresets; ++pi)
    {
        bool ok = true;
        for (size_t i = 0; i < 10; ++i)
        {
            if (std::fabs ((double) knobs[i] - (double) kPresets[pi].knobs[i]) > (double) epsilon)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            return (int) pi + 1;
    }
    return 0;
}

} // namespace bridge::personality
