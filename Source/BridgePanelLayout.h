#pragma once

#include <JuceHeader.h>
#include "BridgeInstrumentStyles.h"

/** Fixed-size regions so Leader / Drums / Bass / Keys / Guitar share the same shell geometry. */
namespace bridge::panelLayout
{

struct InstrumentShell
{
    juce::Rectangle<int> mainCard;
    /** Full-width bottom strip (four columns laid out inside BridgeBottomHalf). */
    juce::Rectangle<int> bottomStrip;
};

/** Content = panel local bounds reduced by outer margin only (callers pass getLocalBounds().reduced(16)). */
inline InstrumentShell splitInstrumentContent (juce::Rectangle<int> content,
                                               int topTrim = 8)
{
    using namespace bridge::instrumentLayout;

    if (topTrim > 0)
        content.removeFromTop (topTrim);

    content.removeFromBottom (kSectionGap);
    auto bottomStrip = content.removeFromBottom (kBottomCardH);

    return { content, bottomStrip };
}

constexpr int kInstrumentComboH = 32; // matches header transport combo height

/** Melodic pages: Style, ROOT / SCALE / OCT, then page M/S (right). */
inline void layoutMelodicDropdownRow (juce::Rectangle<int> row,
                                      juce::Label& styleLab, juce::ComboBox& styleBox,
                                      juce::Label& rootLab, juce::ComboBox& rootBox,
                                      juce::Label& scaleLab, juce::ComboBox& scaleBox,
                                      juce::Label& octLab, juce::ComboBox& octBox,
                                      juce::TextButton& muteBtn, juce::TextButton& soloBtn)
{
    using namespace bridge::instrumentLayout;
    const int h   = kDropdownH;
    const int ch  = kInstrumentComboH;
    const int labY = row.getCentreY() - (int) ((float) h * 0.5f);
    const int comboY = row.getCentreY() - ch / 2;

    auto placeGroup = [&] (juce::Label& lab, int lw, juce::ComboBox& box, int bw)
    {
        lab.setBounds (row.removeFromLeft (lw).withHeight (h).withY (labY));
        row.removeFromLeft (8);
        box.setBounds (row.removeFromLeft (bw).withHeight (ch).withY (comboY));
    };

    placeGroup (styleLab, 42, styleBox, 164);
    row.removeFromLeft (30);
    placeGroup (rootLab,  40, rootBox,  52);
    row.removeFromLeft (30);
    placeGroup (scaleLab, 45, scaleBox, 160);
    row.removeFromLeft (30);
    placeGroup (octLab,   35, octBox,   52);

    const int msSide = 22;
    const int msGap = 6;
    const int msY = row.getCentreY() - msSide / 2;
    soloBtn.setBounds (row.removeFromRight (msSide).withSizeKeepingCentre (msSide, msSide).withY (msY));
    row.removeFromRight (msGap);
    muteBtn.setBounds (row.removeFromRight (msSide).withSizeKeepingCentre (msSide, msSide).withY (msY));
}

/** Drums: Style + page M/S on the right. */
inline void layoutDrumsStyleDropdownRow (juce::Rectangle<int> row,
                                         juce::Label& styleLab, juce::ComboBox& styleBox,
                                         juce::TextButton& muteBtn, juce::TextButton& soloBtn)
{
    using namespace bridge::instrumentLayout;
    const int h   = kDropdownH;
    const int ch  = kInstrumentComboH;
    const int labY = row.getCentreY() - (int) ((float) h * 0.5f);
    const int comboY = row.getCentreY() - ch / 2;

    styleLab.setBounds (row.removeFromLeft (42).withHeight (h).withY (labY));
    row.removeFromLeft (8);
    styleBox.setBounds (row.removeFromLeft (164).withHeight (ch).withY (comboY));

    const int msSide = 22;
    const int msGap = 6;
    const int msY = row.getCentreY() - msSide / 2;
    soloBtn.setBounds (row.removeFromRight (msSide).withSizeKeepingCentre (msSide, msSide).withY (msY));
    row.removeFromRight (msGap);
    muteBtn.setBounds (row.removeFromRight (msSide).withSizeKeepingCentre (msSide, msSide).withY (msY));
}

} // namespace bridge::panelLayout
