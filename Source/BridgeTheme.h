#pragma once

#include <JuceHeader.h>

namespace bridge::theme
{

/** Apply one of 20 curated themes (matches apvtsMain "uiTheme" choice indices). */
void applyThemeChoiceIndex (int index);

juce::Colour background();
juce::Colour cardSurface();
juce::Colour cardOutline();
juce::Colour textPrimary();
juce::Colour textSecondary();
juce::Colour textDim();
juce::Colour knobTrack();

juce::Colour accentLeader();
juce::Colour accentDrums();
juce::Colour accentBass();
juce::Colour accentPiano();
juce::Colour accentGuitar();

} // namespace bridge::theme
