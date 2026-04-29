#include "GuitarPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
#include "BridgePhrase.h"
#include "BridgeScaleHighlight.h"
#include "guitar/GuitarStylePresets.h"

namespace
{
static int currentUnifiedStyleChoiceIndex (juce::AudioProcessorValueTreeState& ap)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("style")))
        return c->getIndex();
    if (ap.getRawParameterValue ("style") != nullptr)
        return (int) *ap.getRawParameterValue ("style");
    return 0;
}
} // namespace
// ─── Refactored ────────────────────────────────────────────────────────────

GuitarPanel::GuitarPanel (BridgeProcessor& p)
    : proc (p),
      midiClipEditor (p,
                      BridgeMidiClipEditor::InstrumentKind::guitar,
                      bridge::colors::accentGuitar(),
                      [this] (float dy) { adjustZoomX (dy); },
                      [this] (float dy) { adjustZoomY (dy); }),
      bottomHalf (p.apvtsGuitar, p.apvtsMain, laf, bridge::colors::accentGuitar(),
        [this] { proc.triggerGuitarGenerate(); },
        [this] (bool active) { proc.guitarEngine.setFillHoldActive (active); }),
      instrumentStrip (InstrumentControlBar::makeGuitarConfig (p))
{
    setLookAndFeel (&laf);

    proc.apvtsMain.addParameterListener ("loopStart", this);
    proc.apvtsMain.addParameterListener ("loopEnd", this);
    proc.apvtsMain.addParameterListener ("playbackLoopOn", this);
    proc.apvtsGuitar.addParameterListener ("tickerSpeed", this);
    proc.apvtsGuitar.addParameterListener ("style", this);
    proc.apvtsGuitar.state.addListener (this);
    proc.apvtsMain.addParameterListener ("guitarOn", this);
    proc.apvtsMain.addParameterListener ("phraseBars", this);
    proc.apvtsMain.addParameterListener ("scale", this);
    proc.apvtsMain.addParameterListener ("rootNote", this);
    proc.apvtsMain.addParameterListener ("octave", this);
    proc.apvtsMain.addParameterListener ("timeDivision", this);
    proc.apvtsGuitar.addParameterListener ("rollSpanOctaves", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity",
                            "life", "melody", "followRhythm", "palmMute", "strumIntensity", "velShape" })
        proc.apvtsGuitar.addParameterListener (id, this);

    addAndMakeVisible (melodicViewport);
    melodicViewport.setViewedComponent (&midiClipEditor, false);
    melodicViewport.setScrollBarsShown (true, true);
    melodicViewport.setScrollBarThickness (10);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (&laf);

    addAndMakeVisible (loopStrip);
    loopStrip.setStepLabelGutter ((int) bridge::kMelodicKeyStripWidth);
    addAndMakeVisible (bottomHalf);
    addAndMakeVisible (instrumentStrip);

    instrumentStrip.getMuteButton().setTooltip ("Mute Guitar");
    instrumentStrip.getSoloButton().setTooltip ("Solo Guitar");
    instrumentStrip.getMuteButton().onClick = [this]
    {
        const bool muted = instrumentStrip.getMuteButton().getToggleState();
        if (auto* par = proc.apvtsMain.getParameter ("guitarOn"))
            par->setValueNotifyingHost (muted ? 0.0f : 1.0f);
    };

    melodicTonalityPrev = std::move (proc.onMelodicTonalityChanged);
    std::function<void()> storedPrev = melodicTonalityPrev;
    proc.onMelodicTonalityChanged = [this, before = std::move (melodicTonalityPrev)] () mutable
    {
        if (before)
            before();
        juce::MessageManager::callAsync ([this]
        {
            resized();
            scrollMelodicViewportToPatternCentre();
            repaint();
        });
    };
    melodicTonalityPrev = std::move (storedPrev);

    proc.rebuildGuitarGridPreview();
    stepTimer.startTimerHz (30);
    applyGuitarPageState();
    applyPhraseBarsToUi();
}

GuitarPanel::~GuitarPanel()
{
    proc.onMelodicTonalityChanged = std::move (melodicTonalityPrev);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (nullptr);
    proc.apvtsMain.removeParameterListener ("guitarOn", this);
    proc.apvtsMain.removeParameterListener ("phraseBars", this);
    proc.apvtsMain.removeParameterListener ("scale", this);
    proc.apvtsMain.removeParameterListener ("rootNote", this);
    proc.apvtsMain.removeParameterListener ("octave", this);
    proc.apvtsMain.removeParameterListener ("timeDivision", this);
    proc.apvtsGuitar.removeParameterListener ("rollSpanOctaves", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsGuitar.removeParameterListener ("tickerSpeed", this);
    proc.apvtsGuitar.removeParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity",
                            "life", "melody", "followRhythm", "palmMute", "strumIntensity", "velShape" })
        proc.apvtsGuitar.removeParameterListener (id, this);
    proc.apvtsGuitar.state.removeListener (this);
}

void GuitarPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, bridge::phrase::kMaxSteps, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void GuitarPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    deferredGuitarGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

int GuitarPanel::currentPhraseBarCount() const
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("phraseBars")))
        return bridge::phrase::phraseBarsFromChoiceIndex (pc->getIndex());
    return bridge::phrase::phraseBarsFromChoiceIndex (0);
}

void GuitarPanel::applyPhraseBarsToUi()
{
    proc.syncGuitarEngineFromAPVTS();
    const int bars        = currentPhraseBarCount();
    const int phraseSteps = bridge::phrase::phraseStepsForBars (bars);
    loopStrip.setBarRepeats (1);
    loopStrip.setNumSteps (phraseSteps);
    proc.guitarEngine.setPhraseBars (bars);
    resized();
    repaint();
}

void GuitarPanel::applyGuitarPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("guitarOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("guitarOn")->load() > 0.5f;

    bottomHalf.setEnabled (on);
    instrumentStrip.setTrackPowered (on);
    melodicViewport.setEnabled (on);
    midiClipEditor.setEnabled (on);
    loopStrip.setEnabled (on);

    const float dim = on ? 1.0f : 0.42f;
    melodicViewport.setAlpha (dim);
    midiClipEditor.setAlpha (dim);
    loopStrip.setAlpha (dim);
    bottomHalf.setAlpha (dim);
}

void GuitarPanel::scrollMelodicViewportToPatternCentre()
{
    int winLo = 0, winHi = 127;
    bridge::applyRollSpanMelodicWindow (proc.guitarEngine, winLo, winHi);
    const auto extent = bridge::getPatternMidiExtent (proc.guitarEngine);
    const int pMin = extent.first;
    const int pMax = extent.second;
    const int midMidi = (pMin <= pMax) ? (pMin + pMax) / 2 : (winLo + winHi) / 2;
    const int spanRows = juce::jmax (1, winHi - winLo + 1);
    const int vpH = juce::jmax (1, melodicViewport.getHeight());
    const int editorH = juce::jmax (1, midiClipEditor.getHeight());
    const int noteH = juce::jmax (1, editorH - BridgeMidiClipEditor::verticalChromePx);
    const float rowH = (float) noteH / (float) spanRows;
    const int maxScroll = juce::jmax (0, editorH - vpH);
    constexpr int kTimeRulerH = 18;
    const float centerY = (float) kTimeRulerH + ((float) (winHi - midMidi) + 0.5f) * rowH;
    const int scrollY = juce::jlimit (0, maxScroll, juce::roundToInt (centerY - (float) vpH * 0.5f));
    melodicViewport.setViewPosition (melodicViewport.getViewPositionX(), scrollY);
}

void GuitarPanel::fitPatternInView()
{
    zoomX = 1.0f;
    zoomY = 1.0f;
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
    scrollMelodicViewportToPatternCentre();
}

void GuitarPanel::adjustZoomX (float delta)
{
    zoomX = juce::jlimit (0.4f, 6.0f, zoomX * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void GuitarPanel::adjustZoomY (float delta)
{
    zoomY = juce::jlimit (0.4f, 6.0f, zoomY * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void GuitarPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto bounds = getLocalBounds();
    auto stripRow = bounds.removeFromTop (kDropdownH);
    instrumentStrip.setBounds (stripRow);
    auto inner = bounds.reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 0);

    auto card = shell.mainCard.reduced (8, 8);
    loopStrip.setBounds (card.removeFromTop ((int) bridge::kLoopRangeStripHeightPx).reduced (4, 0));
    loopStrip.setAccent (bridge::colors::accentGuitar());
    const int phraseSteps = proc.guitarEngine.getPatternLen();
    loopStrip.setNumSteps (phraseSteps);

    melodicViewport.setBounds (card);
    const int viewW = juce::jmax (1, melodicViewport.getWidth());
    const int viewH = juce::jmax (1, melodicViewport.getHeight());

    int minMidi = 60, maxMidi = 72;
    bridge::applyRollSpanMelodicWindow (proc.guitarEngine, minMidi, maxMidi);
    const int nRows  = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = phraseSteps;
    const int strip  = (int) bridge::kMelodicKeyStripWidth;

    const float baseCellW = (float) (viewW - strip) / (float) juce::jmax (1, nSteps);
    const float baseCellH = (float) viewH / (float) nRows;
    const float cellW     = baseCellW * zoomX;
    const float cellH     = baseCellH * zoomY;

    const int bodyW = strip + (int) (cellW * (float) nSteps);
    const int chrome = BridgeMidiClipEditor::verticalChromePx;
    const int bodyH = chrome + juce::jmax (1, (int) (cellH * (float) nRows));

    midiClipEditor.setZoom (zoomX, zoomY);
    midiClipEditor.setSize (bodyW, bodyH);

    bottomHalf.setBounds (shell.bottomStrip);
}

void GuitarPanel::paint (juce::Graphics& g)
{
    using namespace bridge::instrumentLayout;
    g.fillAll (bridge::colors::background());

    auto bounds = getLocalBounds();

    bounds.removeFromTop (kDropdownH);
    auto inner = bounds.reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 0);

    auto drawCard = [&] (juce::Rectangle<int> r)
    {
        auto rf = r.toFloat();
        g.setColour (bridge::colors::cardSurface());
        g.fillRect (rf);
        g.setColour (bridge::colors::cardOutline().withAlpha (0.35f));
        g.drawRect (rf.reduced (0.5f), 1.0f);
    };
    drawCard (shell.mainCard);
}

void GuitarPanel::handleAsyncUpdate()
{
    midiClipEditor.repaint();
    loopStrip.repaint();
}

void GuitarPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "guitarOn")
    {
        applyGuitarPageState();
        proc.rebuildGuitarGridPreview();
        triggerAsyncUpdate();
        return;
    }
    if (parameterID == "phraseBars")
    {
        applyPhraseBarsToUi();
        triggerAsyncUpdate();
        return;
    }
    if (parameterID == "rollSpanOctaves")
        proc.syncGuitarEngineFromAPVTS();

    if (parameterID == "loopStart" || parameterID == "loopEnd" || parameterID == "playbackLoopOn"
        || parameterID == "scale" || parameterID == "rootNote" || parameterID == "octave"
        || parameterID == "timeDivision" || parameterID == "rollSpanOctaves"
        || parameterID == "uiTheme")
    {
        loopStrip.repaint();
        midiClipEditor.repaint();
        resized();
    }

    if (parameterID == "density" || parameterID == "complexity")
    {
        proc.syncGuitarEngineFromAPVTS();
        int ls = 1, le = proc.guitarEngine.getPatternLen();
        proc.getGuitarLoopBounds (ls, le);
        proc.guitarEngine.morphPatternForDensityAndComplexity (
            ls - 1, juce::jmin (le - 1, proc.guitarEngine.getPatternLen() - 1));
        triggerAsyncUpdate();
        return;
    }
    if (parameterID == "style")
    {
        proc.syncGuitarEngineFromAPVTS();
        proc.guitarEngine.adaptPatternToNewStyle (
            bridgeMelodicEngineStyleIndex (currentUnifiedStyleChoiceIndex (proc.apvtsGuitar)));
        triggerAsyncUpdate();
        fitPatternInView();
        return;
    }

    if (parameterID == "swing" || parameterID == "humanize" || parameterID == "hold"
        || parameterID == "velocity" || parameterID == "ghostAmount" || parameterID == "sustain"
        || parameterID == "temperature" || parameterID == "staccato" || parameterID == "intensity"
        || parameterID == "fillRate" || parameterID == "tickerSpeed")
    {
        proc.rebuildGuitarGridPreview();
        triggerAsyncUpdate();
        return;
    }

    deferredGuitarGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

void GuitarPanel::flushDeferredGuitarGridPreviewRebuild()
{
    if (! deferredGuitarGridPreviewRebuild)
        return;
    deferredGuitarGridPreviewRebuild = false;
    proc.rebuildGuitarGridPreview();
    fitPatternInView();
}

void GuitarPanel::updateStepAnimation()
{
    const int step = proc.guitarCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        midiClipEditor.updatePlayhead (step);
    }
}
