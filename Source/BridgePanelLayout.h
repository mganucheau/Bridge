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

} // namespace bridge::panelLayout
