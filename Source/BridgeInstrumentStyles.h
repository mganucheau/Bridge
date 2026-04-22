#pragma once

#include "drums/DrumsStylePresets.h"
#include "bass/BassStylePresets.h"

// Single style list for every instrument tab (matches drum genre list).
inline int bridgeUnifiedStyleCount() { return NUM_STYLES; }
inline const char* const* bridgeUnifiedStyleNames() { return STYLE_NAMES; }

/** Melodic engines only define 8 style presets; map unified UI index → engine table index. */
inline int bridgeMelodicEngineStyleIndex (int unifiedStyleIndex)
{
    const int n = BassPreset::NUM_STYLES;
    return unifiedStyleIndex % n;
}

namespace bridge::instrumentLayout
{
// ── Legacy constants (still referenced by older code paths) ────────────────
/** Same height as plugin header bar (`kHeaderH`) — two menu bars align. */
inline constexpr int kDropdownH   = 45;
/** Strip combo vertical size (InstrumentControlBar, transport combos). */
inline constexpr int kInstrumentComboH = 32;
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

// ── Unified panel layout (v3 redesign) ─────────────────────────────────────
// The editor is a fixed 960 × 740 window. Every tab splits the space below
// the header into three fixed-size containers so switching tabs never shifts
// anything on screen:
//   • dropdown row (Root / Scale / Oct / Style)
//   • main area   (piano roll, drum grid, or Leader mixer)
//   • bottom row  (knobs card on the left + loop/actions card on the right)
inline constexpr int kWindowW          = 960;
inline constexpr int kWindowH          = 740;
inline constexpr int kHeaderH          = 54;      // BRIDGE logo + transport + tab strip
/** Transport + tab chips share one vertical size. */
inline constexpr int kHeaderControlH   = 38;
/** HIG-style 16 pt margins on primary content. */
inline constexpr int kPanelEdge        = 16;

inline constexpr int kMainAreaH        = 313;     // pattern / mixer (reduced when strip matched header height)
inline constexpr int kBottomCardH      = 288;     // four columns: knobs / tension XY / feel XY / loop+actions
inline constexpr int kSectionGap       = 12;      // gap between main grid and bottom strip
inline constexpr int kCardRadius       = 0;
inline constexpr int kCardGap          = 10;      // horizontal space between knobs card and loop card
inline constexpr int kLoopCardW        = 0;       // deprecated (full-width bottom strip)
inline constexpr int kSectionHeaderH   = 16;      // "GROOVE" / "EXPRESSION" / "LOOPING" / "ACTIONS" label
inline constexpr int kBigActionBtnH    = 52;      // GENERATE / FILL / PERFORM squares in the loop card
inline constexpr int kSyncBtnSide      = 34;      // sync toggle between Start and End knobs
}
