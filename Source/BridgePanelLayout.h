#pragma once

#include <JuceHeader.h>
#include "BridgeInstrumentStyles.h"
#include "BridgeInstrumentUI.h"

/** Fixed-size regions so Leader / Drums / Bass / Keys / Guitar share the same shell geometry. */
namespace bridge::panelLayout
{

struct InstrumentShell
{
    juce::Rectangle<int> mainCard;
    juce::Rectangle<int> knobsCard;
    juce::Rectangle<int> loopActionsCard;
};

/** Content = panel local bounds reduced by outer margin only (callers pass getLocalBounds().reduced(16)). */
inline InstrumentShell splitInstrumentContent (juce::Rectangle<int> content,
                                               int topTrim = 8)
{
    using namespace bridge::instrumentLayout;

    if (topTrim > 0)
        content.removeFromTop (topTrim);

    const int gap = kCardGap;
    auto bottom = content.removeFromBottom (kBottomCardH);
    content.removeFromBottom (kSectionGap);

    const int split = content.getWidth() - kLoopCardW - gap;
    auto knobs = bottom.removeFromLeft (split);
    bottom.removeFromLeft (gap);

    return { content, knobs, bottom };
}

/** Same horizontal rhythm as Bass / Keys / Guitar: Style block, spacer, ROOT / SCALE / OCT, power (right). */
inline void layoutMelodicDropdownRow (juce::Rectangle<int> row,
                                      juce::Label& styleLab, juce::ComboBox& styleBox,
                                      juce::Label& rootLab, juce::ComboBox& rootBox,
                                      juce::Label& scaleLab, juce::ComboBox& scaleBox,
                                      juce::Label& octLab, juce::ComboBox& octBox,
                                      PagePowerButton& powerBtn)
{
    using namespace bridge::instrumentLayout;
    const int h = kDropdownH;
    const int labY = row.getCentreY() - (int) ((float) h * 0.5f);

    auto placeGroup = [&] (juce::Label& lab, int lw, juce::ComboBox& box, int bw)
    {
        lab.setBounds (row.removeFromLeft (lw).withHeight (h).withY (labY));
        row.removeFromLeft (4);
        box.setBounds (row.removeFromLeft (bw).withHeight (h).withY (labY));
        row.removeFromLeft (14);
    };

    placeGroup (styleLab, 42, styleBox, 164);
    row.removeFromLeft (30);
    placeGroup (rootLab,  40, rootBox,  52);
    placeGroup (scaleLab, 45, scaleBox, 140);
    placeGroup (octLab,   35, octBox,   52);

    const int powerSide = 30;
    powerBtn.setBounds (row.removeFromRight (powerSide).withSizeKeepingCentre (powerSide, powerSide));
}

/** Drums: Style block + power on the right. */
inline void layoutDrumsStyleDropdownRow (juce::Rectangle<int> row,
                                         juce::Label& styleLab, juce::ComboBox& styleBox,
                                         PagePowerButton& powerBtn)
{
    using namespace bridge::instrumentLayout;
    const int h = kDropdownH;
    const int labY = row.getCentreY() - (int) ((float) h * 0.5f);

    styleLab.setBounds (row.removeFromLeft (42).withHeight (h).withY (labY));
    row.removeFromLeft (4);
    styleBox.setBounds (row.removeFromLeft (164).withHeight (h).withY (labY));
    row.removeFromLeft (14);

    const int powerSide = 30;
    powerBtn.setBounds (row.removeFromRight (powerSide).withSizeKeepingCentre (powerSide, powerSide));
}

} // namespace bridge::panelLayout
