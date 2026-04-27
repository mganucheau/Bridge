#include "BridgeTheme.h"

namespace bridge::theme
{
namespace detail
{
struct T
{
    juce::Colour bg, card, outline, tp, ts, td, knob;
    juce::Colour al, ad, ab, ap, ag;
};

static T make (juce::uint32 bg, juce::uint32 card, juce::uint32 out,
               juce::uint32 tp, juce::uint32 ts, juce::uint32 td, juce::uint32 knob,
               juce::uint32 al, juce::uint32 ad, juce::uint32 ab, juce::uint32 ap, juce::uint32 ag)
{
    return { juce::Colour (bg), juce::Colour (card), juce::Colour (out),
             juce::Colour (tp), juce::Colour (ts), juce::Colour (td), juce::Colour (knob),
             juce::Colour (al), juce::Colour (ad), juce::Colour (ab), juce::Colour (ap), juce::Colour (ag) };
}

// 10 light + 10 dark pairs — distinct accent curves per row.
static const T kTable[20] = {
    make (0xFFF5F5F7, 0xFFFFFFFF, 0xFFC6C6C8, 0xFF1C1C1E, 0xFF636366, 0xFF8E8E93, 0xFFE5E5EA,
          0xFFC7A500, 0xFFD65A3A, 0xFF2A9D8F, 0xFF9B59D0, 0xFF2563EB),
    make (0xFF1C1C1E, 0xFF2C2C2E, 0xFF48484A, 0xFFFFFFFF, 0xFFAEAEB2, 0xFF636366, 0xFF3A3A3C,
          0xFFFFD60A, 0xFFFF7F5C, 0xFF5ED4C4, 0xFFBF5AF2, 0xFF0A84FF),

    make (0xFFEFF7F0, 0xFFFFFFFF, 0xFFB8D4BE, 0xFF1B1F1C, 0xFF4A5A4E, 0xFF6F7F72, 0xFFDDE8E0,
          0xFF2E7D32, 0xFFE65100, 0xFF00897B, 0xFF6A1B9A, 0xFF1565C0),
    make (0xFF0D1F14, 0xFF153223, 0xFF2D4A38, 0xFFE8F5E9, 0xFFA5D6A7, 0xFF66BB6A, 0xFF1B3A28,
          0xFF81C784, 0xFFFFAB91, 0xFF4DD0E1, 0xFFCE93D8, 0xFF64B5F6),

    make (0xFFF3F7FD, 0xFFFFFFFF, 0xFFC5D3EB, 0xFF1A2333, 0xFF5A6578, 0xFF7A8699, 0xFFE3EAF7,
          0xFF0078D4, 0xFFD83B01, 0xFF00A4A6, 0xFF8764B8, 0xFF2B579A),
    make (0xFF0B1220, 0xFF121C2E, 0xFF2A3A55, 0xFFEAF0FF, 0xFF9DB0D0, 0xFF6B7C99, 0xFF1E2A40,
          0xFF60A5FA, 0xFFFF8A65, 0xFF2DD4BF, 0xFFC084FC, 0xFF38BDF8),

    make (0xFFF7F4F9, 0xFFFFFFFF, 0xFFD4C8E0, 0xFF231826, 0xFF6A5A72, 0xFF8E7E99, 0xFFEDE4F4,
          0xFF7B1FA2, 0xFFE53935, 0xFF00838F, 0xFFAD1457, 0xFF3949AB),
    make (0xFF1A1024, 0xFF261933, 0xFF403352, 0xFFF3E5FF, 0xFFB39DDB, 0xFF7E57C2, 0xFF2D2240,
          0xFFD500F9, 0xFFFF6E40, 0xFF1DE9B6, 0xFFFF4081, 0xFF448AFF),

    make (0xFFFDF7ED, 0xFFFFFFFF, 0xFFE2D2B8, 0xFF2A2115, 0xFF7A6A52, 0xFF9A8A72, 0xFFF5E9D6,
          0xFFE65100, 0xFFC62828, 0xFF00695C, 0xFF6A1B9A, 0xFF0277BD),
    make (0xFF1F140A, 0xFF2E1F12, 0xFF4A3624, 0xFFFFF3E0, 0xFFFFCC80, 0xFFFFB74D, 0xFF3E2A18,
          0xFFFFB300, 0xFFFF5252, 0xFF26C6DA, 0xFFEA80FC, 0xFF40C4FF),

    make (0xFFF4FBFF, 0xFFFFFFFF, 0xFFC8E6F5, 0xFF0F2430, 0xFF4A6575, 0xFF6F8A9A, 0xFFE0F2FA,
          0xFF0097A7, 0xFFFF6D00, 0xFF00ACC1, 0xFF8E24AA, 0xFF1565C0),
    make (0xFF061A22, 0xFF0C2833, 0xFF1F4556, 0xFFE0F7FA, 0xFF80DEEA, 0xFF4DD0E1, 0xFF123844,
          0xFF18FFFF, 0xFFFF9100, 0xFF1DE9B6, 0xFFEA80FC, 0xFF82B1FF),

    make (0xFFF5F0FF, 0xFFFFFFFF, 0xFFD8CFF5, 0xFF1F1733, 0xFF6A5F80, 0xFF8E8499, 0xFFEDE7FF,
          0xFF5E35B1, 0xFFD84315, 0xFF00796B, 0xFFC2185B, 0xFF1565C0),
    make (0xFF12081F, 0xFF1E1030, 0xFF35264D, 0xFFEDE7F6, 0xFFB39DDB, 0xFF9575CD, 0xFF241838,
          0xFFB388FF, 0xFFFF8A65, 0xFF64FFDA, 0xFFFF80AB, 0xFF82B1FF),

    make (0xFFF2F8F2, 0xFFFFFFFF, 0xFFC9DEC9, 0xFF142414, 0xFF4F6A4F, 0xFF6F8A6F, 0xFFE2EEE2,
          0xFF33691E, 0xFFB71C1C, 0xFF006064, 0xFF6A1B9A, 0xFF0D47A1),
    make (0xFF0E160E, 0xFF1A261A, 0xFF2F3F2F, 0xFFE8F5E9, 0xFFA5D6A7, 0xFF81C784, 0xFF1C2A1C,
          0xFF76FF03, 0xFFFF5252, 0xFF1DE9B6, 0xFFE040FB, 0xFF448AFF),

    make (0xFFFFF5F5, 0xFFFFFFFF, 0xFFF5C6C6, 0xFF2A1212, 0xFF7A5A5A, 0xFF9A7A7A, 0xFFFCE4EC,
          0xFFC62828, 0xFFFF6F00, 0xFF00838F, 0xFFAD1457, 0xFF0D47A1),
    make (0xFF1A0A0A, 0xFF2A1414, 0xFF4A2A2A, 0xFFFFEBEE, 0xFFFFCDD2, 0xFFFF8A80, 0xFF3A2020,
          0xFFFF5252, 0xFFFFAB40, 0xFF64FFDA, 0xFFFF80AB, 0xFF40C4FF),

    make (0xFFF5F8FA, 0xFFFFFFFF, 0xFFC9D6DE, 0xFF0F1B22, 0xFF4A5A64, 0xFF6F7F89, 0xFFE1E9EE,
          0xFF455A64, 0xFFD84315, 0xFF00695C, 0xFF6A1B9A, 0xFF0277BD),
    make (0xFF0A1216, 0xFF141C22, 0xFF2A3640, 0xFFECEFF1, 0xFFB0BEC5, 0xFF78909C, 0xFF1C2429,
          0xFF90A4AE, 0xFFFF8A65, 0xFF4DD0E1, 0xFFCE93D8, 0xFF64B5F6),
};

static T g = kTable[1];
} // namespace detail

void applyThemeChoiceIndex (int)
{
    // Single supported palette: first dark row (Material-style).
    detail::g = detail::kTable[1];
}

juce::Colour background()    { return detail::g.bg; }
juce::Colour cardSurface()   { return detail::g.card; }
juce::Colour cardOutline()   { return detail::g.outline; }
juce::Colour textPrimary()   { return detail::g.tp; }
juce::Colour textSecondary() { return detail::g.ts; }
juce::Colour textDim()       { return detail::g.td; }
juce::Colour knobTrack()     { return detail::g.knob; }

juce::Colour accentLeader()  { return detail::g.al; }
juce::Colour accentDrums()   { return detail::g.ad; }
juce::Colour accentBass()    { return detail::g.ab; }
juce::Colour accentPiano()   { return detail::g.ap; }
juce::Colour accentGuitar()  { return detail::g.ag; }

} // namespace bridge::theme
