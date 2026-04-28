#pragma once

#include <JuceHeader.h>

namespace bridge::phrase
{
// One bar is always a 16th-note grid with 16 steps.
inline constexpr int kStepsPerBar   = 16;
inline constexpr int kMaxPhraseBars = 16;
inline constexpr int kMaxSteps      = kStepsPerBar * kMaxPhraseBars; // 256

/** Main header / jam period: idx 0=2, 1=4, 2=8 bars. Legacy idx 3 (16 bars) maps to 8. */
inline int phraseBarsFromChoiceIndex (int idx) noexcept
{
    idx = juce::jlimit (0, 3, idx);
    if (idx > 2)
        idx = 2;
    static constexpr int kBars[3] = { 2, 4, 8 };
    return kBars[(size_t) idx];
}

/** Jam period dropdown — same bar counts as main phrase (indices 0..3). */
inline int jamPeriodBarsFromChoiceIndex (int idx) noexcept
{
    return phraseBarsFromChoiceIndex (idx);
}

/** Header `timeDivision` choice index 0..9 → highlight every N sixteenth-steps within a bar. */
inline int accentColumnPeriodInSixteenthsFromTimeDivisionIndex (int idx) noexcept
{
    idx = juce::jlimit (0, 9, idx);
    static constexpr int k[10] = { 1, 1, 1, 1, 2, 2, 2, 4, 8, 16 };
    return juce::jmax (1, k[(size_t) idx]);
}

inline int readTimeDivisionChoiceIndex (juce::AudioProcessorValueTreeState& mainApvts) noexcept
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (mainApvts.getParameter ("timeDivision")))
        return p->getIndex();
    if (auto* v = mainApvts.getRawParameterValue ("timeDivision"))
        return juce::jlimit (0, 9, (int) v->load());
    return 3;
}

/** Colours + property tag to match header phrase/time combos (no editor chrome LAF required). */
inline void stylePhraseLengthComboBox (juce::ComboBox& box)
{
    box.getProperties().set ("bridgeHeaderStrip", true);
    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2a));
    box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.2f));
    box.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.55f));
}

inline void addPhraseLengthBarItems (juce::ComboBox& box)
{
    box.clear (juce::dontSendNotification);
    box.addItem ("2 bars", 1);
    box.addItem ("4 bars", 2);
    box.addItem ("8 bars", 3);
}

inline int phraseStepsForBars (int bars) noexcept
{
    return juce::jlimit (kStepsPerBar, kMaxSteps, bars * kStepsPerBar);
}

inline int clampStep1BasedToPhrase (int step1, int phraseSteps) noexcept
{
    return juce::jlimit (1, juce::jmax (1, phraseSteps), step1);
}

/** Total 16th-note steps for the current main `phraseBars` choice (2 / 4 / 8 bars). */
inline int readPhraseStepCount (juce::AudioProcessorValueTreeState& mainApvts) noexcept
{
    int bars = phraseBarsFromChoiceIndex (0);
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (mainApvts.getParameter ("phraseBars")))
        bars = phraseBarsFromChoiceIndex (pc->getIndex());
    else if (auto* v = mainApvts.getRawParameterValue ("phraseBars"))
        bars = phraseBarsFromChoiceIndex ((int) v->load());
    return phraseStepsForBars (bars);
}
} // namespace bridge::phrase
