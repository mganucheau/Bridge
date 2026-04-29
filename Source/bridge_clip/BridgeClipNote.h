#pragma once

#include <cstdint>

/** One scheduled MIDI event in phrase-local PPQ space (JUCE host ppq = quarter notes from song start).
    `stepIndex0` is the 0-based pattern step (16th grid). `microPpq` is additional offset from that
    step boundary (swing + humanize + anticipation), matching BridgeProcessor scheduling math.
    General MIDI drum pitches: see DRUM_MIDI_NOTES in drums/DrumsStylePresets.h (Kick=36 … Ride=51). */
struct BridgeClipNote
{
    int      stepIndex0 = 0;
    double   microPpq   = 0.0;
    uint8_t  pitch      = 60;
    uint8_t  velocity   = 100;
    double   lengthPpq  = 0.25;
    uint8_t  flags      = 0;

    static constexpr uint8_t kGhostFlag   = 1 << 0;
    static constexpr uint8_t kDrumLaneMask = 0x1f << 1;

    bool isGhost() const noexcept { return (flags & kGhostFlag) != 0; }
    void setGhost (bool g) noexcept
    {
        if (g) flags |= kGhostFlag;
        else   flags = (uint8_t) (flags & ~kGhostFlag);
    }
    int drumLane() const noexcept { return (int) ((flags & kDrumLaneMask) >> 1) - 1; }
    void setDrumLane (int lane0to8) noexcept
    {
        lane0to8 = lane0to8 < -1 ? -1 : (lane0to8 > 8 ? 8 : lane0to8);
        flags = (uint8_t) ((flags & ~kDrumLaneMask) | (((uint8_t) (lane0to8 + 1) & 0x1f) << 1));
    }
};
