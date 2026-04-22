#include "BridgeInstrumentTabTile.h"
#include "BridgeAppleHIG.h"

namespace
{
static bool readPowerOn (juce::AudioProcessorValueTreeState& ap, const juce::String& id)
{
    if (auto* v = ap.getRawParameterValue (id))
        return v->load() > 0.5f;
    return true;
}
} // namespace

BridgeInstrumentTabTile::BridgeInstrumentTabTile (BridgeProcessor& processor,
                                                int tabIndexInEditor,
                                                const juce::String& shortCaption,
                                                const juce::String& powerParameterId,
                                                juce::Colour accentColour,
                                                std::function<void (int)> onSelectTab)
    : proc (processor),
      tabIndex (tabIndexInEditor),
      caption (shortCaption),
      powerParamId (powerParameterId),
      accent (accentColour),
      onSelect (std::move (onSelectTab))
{
    setOpaque (true);
    setInterceptsMouseClicks (true, true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
    proc.apvtsMain.addParameterListener (powerParamId, this);
}

BridgeInstrumentTabTile::~BridgeInstrumentTabTile()
{
    proc.apvtsMain.removeParameterListener (powerParamId, this);
}

void BridgeInstrumentTabTile::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == powerParamId)
        repaint();
}

void BridgeInstrumentTabTile::setTileSelected (bool isSelectedTab)
{
    if (selected == isSelectedTab)
        return;
    selected = isSelectedTab;
    repaint();
}

void BridgeInstrumentTabTile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    const bool engineOn = readPowerOn (proc.apvtsMain, powerParamId);
    const bool powered  = (tabIndex == 0) || engineOn;

    if (! powered)
        g.saveState();

    if (! powered)
        g.setOpacity (0.5f);

    const juce::Colour inactiveBg (0xff2a2a2a);
    const juce::Colour inactiveBorder (0xff3a3a3a);

    if (selected)
    {
        g.setColour (accent.withAlpha (0.20f));
        g.fillRect (r);
        g.setColour (accent);
        g.drawRect (r.reduced (0.5f), 1.0f);
    }
    else
    {
        g.setColour (inactiveBg);
        g.fillRect (r);
        g.setColour (inactiveBorder);
        g.drawRect (r.reduced (0.5f), 1.0f);
    }

    g.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
    g.setColour (powered ? juce::Colours::white : juce::Colour (0xff666666));
    g.drawText (caption, getLocalBounds(), juce::Justification::centred, false);

    if (! powered)
        g.restoreState();
}

void BridgeInstrumentTabTile::mouseDown (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (onSelect)
        onSelect (tabIndex);
}
