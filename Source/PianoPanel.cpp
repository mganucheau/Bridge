#include "PianoPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
#include "BridgePhrase.h"
#include "BridgeScaleHighlight.h"
#include "piano/PianoStylePresets.h"
#include "BridgeInstrumentStyles.h"

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

PianoPanel::PianoPanel (BridgeProcessor& p)
    : proc (p),
      midiClipEditor (p,
                      BridgeMidiClipEditor::InstrumentKind::piano,
                      bridge::colors::accentPiano(),
                      [this] (float dy) { adjustZoomX (dy); },
                      [this] (float dy) { adjustZoomY (dy); }),
      bottomHalf (p.apvtsPiano, p.apvtsMain, laf, bridge::colors::accentPiano(),
        [this] { proc.triggerPianoGenerate(); },
        [this] (bool active) { proc.pianoEngine.setFillHoldActive (active); }),
      instrumentStrip (InstrumentControlBar::makePianoConfig (p))
{
    setLookAndFeel (&laf);

    proc.apvtsMain.addParameterListener ("loopStart", this);
    proc.apvtsMain.addParameterListener ("loopEnd", this);
    proc.apvtsMain.addParameterListener ("playbackLoopOn", this);
    proc.apvtsPiano.addParameterListener ("tickerSpeed", this);
    proc.apvtsPiano.addParameterListener ("style", this);
    proc.apvtsPiano.state.addListener (this);
    proc.apvtsMain.addParameterListener ("pianoOn", this);
    proc.apvtsMain.addParameterListener ("phraseBars", this);
    proc.apvtsMain.addParameterListener ("scale", this);
    proc.apvtsMain.addParameterListener ("rootNote", this);
    proc.apvtsMain.addParameterListener ("octave", this);
    proc.apvtsMain.addParameterListener ("timeDivision", this);
    proc.apvtsPiano.addParameterListener ("rollSpanOctaves", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity",
                            "life", "melody", "followRhythm", "voicingSpread", "velShape" })
        proc.apvtsPiano.addParameterListener (id, this);

    addAndMakeVisible (melodicViewport);
    melodicViewport.setViewedComponent (&midiClipEditor, false);
    melodicViewport.setScrollBarsShown (true, true);
    melodicViewport.setScrollBarThickness (10);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (&laf);

    addAndMakeVisible (loopStrip);
    loopStrip.setStepLabelGutter ((int) bridge::kMelodicKeyStripWidth);
    addAndMakeVisible (bottomHalf);
    addAndMakeVisible (instrumentStrip);

    instrumentStrip.getMuteButton().setTooltip ("Mute Keys");
    instrumentStrip.getSoloButton().setTooltip ("Solo Keys");
    instrumentStrip.getMuteButton().onClick = [this]
    {
        const bool muted = instrumentStrip.getMuteButton().getToggleState();
        if (auto* par = proc.apvtsMain.getParameter ("pianoOn"))
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

    proc.rebuildPianoGridPreview();
    stepTimer.startTimerHz (30);
    applyPianoPageState();
    applyPhraseBarsToUi();
}

PianoPanel::~PianoPanel()
{
    proc.onMelodicTonalityChanged = std::move (melodicTonalityPrev);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (nullptr);
    proc.apvtsMain.removeParameterListener ("pianoOn", this);
    proc.apvtsMain.removeParameterListener ("phraseBars", this);
    proc.apvtsMain.removeParameterListener ("scale", this);
    proc.apvtsMain.removeParameterListener ("rootNote", this);
    proc.apvtsMain.removeParameterListener ("octave", this);
    proc.apvtsMain.removeParameterListener ("timeDivision", this);
    proc.apvtsPiano.removeParameterListener ("rollSpanOctaves", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsPiano.removeParameterListener ("tickerSpeed", this);
    proc.apvtsPiano.removeParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity",
                            "life", "melody", "followRhythm", "voicingSpread", "velShape" })
        proc.apvtsPiano.removeParameterListener (id, this);
    proc.apvtsPiano.state.removeListener (this);
}

void PianoPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, bridge::phrase::kMaxSteps, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void PianoPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    deferredPianoGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

int PianoPanel::currentPhraseBarCount() const
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("phraseBars")))
        return bridge::phrase::phraseBarsFromChoiceIndex (pc->getIndex());
    return bridge::phrase::phraseBarsFromChoiceIndex (0);
}

void PianoPanel::applyPhraseBarsToUi()
{
    proc.syncPianoEngineFromAPVTS();
    const int bars        = currentPhraseBarCount();
    const int phraseSteps = bridge::phrase::phraseStepsForBars (bars);
    loopStrip.setBarRepeats (1);
    loopStrip.setNumSteps (phraseSteps);
    proc.pianoEngine.setPhraseBars (bars);
    resized();
    repaint();
}

void PianoPanel::applyPianoPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("pianoOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("pianoOn")->load() > 0.5f;

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

void PianoPanel::scrollMelodicViewportToPatternCentre()
{
    int winLo = 0, winHi = 127;
    bridge::applyRollSpanMelodicWindow (proc.pianoEngine, winLo, winHi);
    const auto extent = bridge::getPatternMidiExtent (proc.pianoEngine);
    const int pMin = extent.first;
    const int pMax = extent.second;
    const int midMidi = (pMin <= pMax) ? (pMin + pMax) / 2 : (winLo + winHi) / 2;
    bridge::scrollMelodicClipViewportCentreOnPitch (melodicViewport,
                                                   midiClipEditor.getHeight(),
                                                   BridgeMidiClipEditor::verticalChromePx,
                                                   BridgeMidiClipEditor::kTimeRulerHeightPx,
                                                   winLo, winHi, midMidi);
}

void PianoPanel::fitPatternInView()
{
    zoomX = 1.0f;
    zoomY = 1.0f;
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
    scrollMelodicViewportToPatternCentre();
}

void PianoPanel::adjustZoomX (float delta)
{
    zoomX = juce::jlimit (0.4f, 6.0f, zoomX * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void PianoPanel::adjustZoomY (float delta)
{
    zoomY = juce::jlimit (0.4f, 6.0f, zoomY * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void PianoPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto bounds = getLocalBounds();
    auto stripRow = bounds.removeFromTop (kDropdownH);
    instrumentStrip.setBounds (stripRow);
    auto inner = bounds.reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 0);

    auto card = shell.mainCard.reduced (8, 8);
    loopStrip.setBounds (card.removeFromTop ((int) bridge::kLoopRangeStripHeightPx).reduced (4, 0));
    loopStrip.setAccent (bridge::colors::accentPiano());
    const int phraseSteps = proc.pianoEngine.getPatternLen();
    loopStrip.setNumSteps (phraseSteps);

    melodicViewport.setBounds (card);
    const int viewW = juce::jmax (1, melodicViewport.getWidth());
    const int viewH = juce::jmax (1, melodicViewport.getHeight());

    int minMidi = 60, maxMidi = 72;
    bridge::applyRollSpanMelodicWindow (proc.pianoEngine, minMidi, maxMidi);
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

void PianoPanel::paint (juce::Graphics& g)
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

void PianoPanel::handleAsyncUpdate()
{
    midiClipEditor.repaint();
    loopStrip.repaint();
}

void PianoPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "pianoOn")
    {
        applyPianoPageState();
        proc.rebuildPianoGridPreview();
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
        proc.syncPianoEngineFromAPVTS();

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
        proc.syncPianoEngineFromAPVTS();
        int ls = 1, le = proc.pianoEngine.getPatternLen();
        proc.getPianoLoopBounds (ls, le);
        proc.pianoEngine.morphPatternForDensityAndComplexity (
            ls - 1, juce::jmin (le - 1, proc.pianoEngine.getPatternLen() - 1));
        triggerAsyncUpdate();
        return;
    }
    if (parameterID == "style")
    {
        proc.syncPianoEngineFromAPVTS();
        proc.pianoEngine.adaptPatternToNewStyle (
            bridgeMelodicEngineStyleIndex (currentUnifiedStyleChoiceIndex (proc.apvtsPiano)));
        triggerAsyncUpdate();
        fitPatternInView();
        return;
    }

    if (parameterID == "swing" || parameterID == "humanize" || parameterID == "hold"
        || parameterID == "velocity" || parameterID == "ghostAmount" || parameterID == "sustain"
        || parameterID == "temperature" || parameterID == "staccato" || parameterID == "intensity"
        || parameterID == "fillRate" || parameterID == "tickerSpeed")
    {
        proc.rebuildPianoGridPreview();
        triggerAsyncUpdate();
        return;
    }

    deferredPianoGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

void PianoPanel::flushDeferredPianoGridPreviewRebuild()
{
    if (! deferredPianoGridPreviewRebuild)
        return;
    deferredPianoGridPreviewRebuild = false;
    proc.rebuildPianoGridPreview();
    fitPatternInView();
}

void PianoPanel::updateStepAnimation()
{
    const int step = proc.pianoCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        midiClipEditor.updatePlayhead (step);
    }
}
