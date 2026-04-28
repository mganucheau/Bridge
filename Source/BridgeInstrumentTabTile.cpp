#include "BridgeInstrumentTabTile.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"

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
                                                std::function<void (int)> onSelectTab,
                                                const juce::String& mixerMuteId,
                                                const juce::String& mixerSoloId)
    : proc (processor),
      tabIndex (tabIndexInEditor),
      caption (shortCaption),
      powerParamId (powerParameterId),
      mixerMuteParamId (mixerMuteId),
      mixerSoloParamId (mixerSoloId),
      accent (accentColour),
      onSelect (std::move (onSelectTab))
{
    setOpaque (true);
    setInterceptsMouseClicks (true, true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
    proc.apvtsMain.addParameterListener (powerParamId, this);
    if (mixerMuteParamId.isNotEmpty())
        proc.apvtsMain.addParameterListener (mixerMuteParamId, this);
    if (mixerSoloParamId.isNotEmpty())
        proc.apvtsMain.addParameterListener (mixerSoloParamId, this);
}

BridgeInstrumentTabTile::~BridgeInstrumentTabTile()
{
    proc.apvtsMain.removeParameterListener (powerParamId, this);
    if (mixerMuteParamId.isNotEmpty())
        proc.apvtsMain.removeParameterListener (mixerMuteParamId, this);
    if (mixerSoloParamId.isNotEmpty())
        proc.apvtsMain.removeParameterListener (mixerSoloParamId, this);
}

void BridgeInstrumentTabTile::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == powerParamId || parameterID == mixerMuteParamId || parameterID == mixerSoloParamId)
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
    auto r = getLocalBounds().toFloat();

    const bool engineOn = readPowerOn (proc.apvtsMain, powerParamId);
    const bool powered  = (tabIndex == 0) || engineOn;

    auto mixerFlagOn = [] (juce::AudioProcessorValueTreeState& ap, const juce::String& id) -> bool
    {
        if (id.isEmpty() || ap.getRawParameterValue (id) == nullptr)
            return false;
        return ap.getRawParameterValue (id)->load() > 0.5f;
    };
    const bool rowMuted = mixerFlagOn (proc.apvtsMain, mixerMuteParamId);
    const bool rowSolo  = mixerFlagOn (proc.apvtsMain, mixerSoloParamId);

    if (! powered)
        g.saveState();

    if (! powered)
        g.setOpacity (0.5f);

    const auto chipFill = bridge::colors::cardSurface();
    const auto chipFillSelected = chipFill.brighter (0.045f);
    const juce::Colour borderUnsel = bridge::colors::cardOutline();
    const juce::Colour borderSel =
        accent.interpolatedWith (borderUnsel, 0.35f).withAlpha (1.0f);

    if (selected)
    {
        g.setColour (chipFillSelected);
        g.fillRect (r);
        g.setColour (borderSel);
        g.drawRect (r.reduced (0.5f), 1.5f);
    }
    else
    {
        g.setColour (chipFill);
        g.fillRect (r);
        g.setColour (borderUnsel);
        g.drawRect (r.reduced (0.5f), 1.0f);
    }

    g.setFont (bridge::hig::uiFont (11.0f, selected ? "Bold" : "Semibold"));
    juce::Colour textCol = powered ? juce::Colours::white.withAlpha (0.95f) : juce::Colour (0xff8a8a8a);
    if (powered && rowMuted)
        textCol = juce::Colour (0xffffb74d).withAlpha (0.95f);
    else if (powered && rowSolo)
        textCol = juce::Colour (0xff82b1ff).withAlpha (0.98f);
    g.setColour (textCol);
    g.drawText (caption, getLocalBounds().reduced (2, 0), juce::Justification::centred, false);

    if (rowMuted || rowSolo)
    {
        const float badge = 9.0f;
        auto br = r.removeFromBottom (badge + 3.0f).removeFromRight (badge + 3.0f)
                      .withSizeKeepingCentre (badge, badge);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (br.translated (0.5f, 0.5f), 2.0f);
        g.setColour (rowSolo ? juce::Colour (0xff82b1ff) : juce::Colour (0xffffb74d));
        g.fillRoundedRectangle (br, 2.0f);
        g.setFont (bridge::hig::uiFont (7.5f, "Bold"));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawText (rowSolo ? "S" : "M", br.toNearestInt(), juce::Justification::centred, false);
    }

    if (! powered)
        g.restoreState();
}

void BridgeInstrumentTabTile::mouseDown (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (onSelect)
        onSelect (tabIndex);
}
