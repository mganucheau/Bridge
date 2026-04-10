#pragma once

#include "animal/AnimalStylePresets.h"
#include "bootsy/BootsyStylePresets.h"

// Single style list for every instrument tab (matches drum genre list).
inline int bridgeUnifiedStyleCount() { return NUM_STYLES; }
inline const char* const* bridgeUnifiedStyleNames() { return STYLE_NAMES; }

/** Melodic engines only define 8 style presets; map unified UI index → engine table index. */
inline int bridgeMelodicEngineStyleIndex (int unifiedStyleIndex)
{
    const int n = BootsyPreset::NUM_STYLES;
    return unifiedStyleIndex % n;
}

namespace bridge::instrumentLayout
{
inline constexpr int kDropdownH   = 28;
inline constexpr int kKnobRowH    = 86;
inline constexpr int kDropdownRow = 34;
inline constexpr int kLoopRowH    = 100;
inline constexpr int kGap         = 8;
/** @deprecated Prefer kActionBtnSide — kept for older references */
inline constexpr int kActionSquare = 44;
/** Compact square GEN / FILL / PERF (~one-third of former 44px control). */
inline constexpr int kActionBtnSide   = 15;
inline constexpr int kActionBtnGap    = 10;
inline constexpr int kSpeedBlockW     = 118;
inline constexpr int kSpeedBtnGap     = 4;
/** PERF blink: half-period in step-timer ticks (30 Hz → 20 ≈ 0.67 s). */
inline constexpr int kPerformBlinkTicks = 20;
}
