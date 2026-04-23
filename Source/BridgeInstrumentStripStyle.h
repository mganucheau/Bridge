#pragma once

#include <JuceHeader.h>
#include "BridgeAppleHIG.h"

/** Shared styling for the top chrome bar + second instrument row (reference UI). */
namespace bridge::instrumentStripStyle
{

/** Reference header row (transport / logo). */
inline juce::Colour headerBarColour() { return juce::Colour (0xff121212); }
/** Slightly darker strip under the transport bar (STYLE / ROOT / …). */
inline juce::Colour rowBackground()  { return juce::Colour (0xff0d0d0d); }
/** Second header / instrument control row (#1A1A1A). */
inline juce::Colour controlBarBackground() { return juce::Colour (0xff1a1a1a); }

inline juce::Colour labelColour()    { return juce::Colours::white.withAlpha (0.6f); }
inline juce::Colour fieldBg()        { return juce::Colour (0xff2a2a2a); }
inline juce::Colour fieldBorder()    { return juce::Colours::white.withAlpha (0.2f); }
inline juce::Colour fieldHoverBg()   { return juce::Colour (0xff3a3a3a); }
/** Gold accent (BRIDGE bar, active tab). */
inline juce::Colour goldAccent()     { return juce::Colour (0xffffc107); }
inline juce::Colour muteOn()         { return juce::Colour (0xffef4444); }
inline juce::Colour soloOn()         { return juce::Colour (0xff3b82f6); }
inline juce::Colour msOffText()      { return juce::Colour (0xff888888); }

inline void tagStripCombo (juce::ComboBox& box)
{
    box.getProperties().set ("bridgeInstrumentStrip", true);
    box.getProperties().set ("bridgeHeaderStrip", true);
}

inline void tagStripMute (juce::TextButton& b) { b.getProperties().set ("bridgeStripMs", "mute"); }
inline void tagStripSolo (juce::TextButton& b) { b.getProperties().set ("bridgeStripMs", "solo"); }

/** Bottom-half loop / span-lock toggles: same chrome as M/S, single-letter label. */
inline void tagStripLoop (juce::TextButton& b)
{
    b.getProperties().remove ("bridgeStripMs");
    b.getProperties().set ("bridgeStripTag", "loop");
}

inline void tagStripSpanLock (juce::TextButton& b)
{
    b.getProperties().remove ("bridgeStripMs");
    b.getProperties().set ("bridgeStripTag", "spanLock");
}

inline void applyStripComboColours (juce::ComboBox& box)
{
    tagStripCombo (box);
    box.setColour (juce::ComboBox::backgroundColourId, fieldBg());
    box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour (juce::ComboBox::outlineColourId, fieldBorder());
    box.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.55f));
}

inline void applyStripLabel (juce::Label& lab, const juce::String& title)
{
    lab.setText (title.toUpperCase(), juce::dontSendNotification);
    lab.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
    lab.setMinimumHorizontalScale (1.0f);
    lab.setJustificationType (juce::Justification::centredRight);
    lab.setColour (juce::Label::textColourId, labelColour());
}

} // namespace bridge::instrumentStripStyle
