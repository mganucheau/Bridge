#include "PianoPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
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

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.pianoEngine.setFillHoldActive (true);
            proc.rebuildPianoGridPreview();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.pianoEngine.setFillHoldActive (false);
            proc.rebuildPianoGridPreview();
        }
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── PianoMelodicBody ───────────────────────────────────────────────────────────

PianoMelodicBody::PianoMelodicBody (PianoPanel& panel, BridgeProcessor& processor)
    : roll (processor), grid (panel, processor)
{
    addAndMakeVisible (roll);
    addAndMakeVisible (grid);
}

void PianoMelodicBody::setMelodicCellSize (float cellW, float cellH)
{
    layoutCellW = cellW;
    layoutCellH = cellH;
}

void PianoMelodicBody::resized()
{
    const int strip = (int) bridge::kMelodicKeyStripWidth;
    const int h       = juce::jmax (1, getHeight());
    const int gridW   = juce::jmax (1, getWidth() - strip);
    roll.setCellSize (layoutCellW, layoutCellH);
    grid.setCellSize (layoutCellW, layoutCellH);
    roll.setBounds (0, 0, strip, h);
    grid.setBounds (strip, 0, gridW, h);
}

// ─── PianoPianoRollComponent ───────────────────────────────────────────────────────

PianoPianoRollComponent::PianoPianoRollComponent (BridgeProcessor& p) : proc (p) {}

void PianoPianoRollComponent::setCellSize (float w, float h)
{
    storedCellW = w;
    storedCellH = h;
}

void PianoPianoRollComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.pianoEngine;
    int low = 60, high = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, low, high);
    const int nRows = juce::jmax (1, high - low + 1);
    const float rowH = storedCellH > 0.01f ? storedCellH : ((float) getHeight() / (float) nRows);
    const int row = (int) (e.position.y / juce::jmax (1.0f, rowH));
    const int idx = juce::jlimit (0, nRows - 1, row);
    const int midi = high - idx;
    proc.queueMelodicPreviewNote (proc.apvtsPiano.getRawParameterValue ("midiChannel") != nullptr
                                      ? (int) *proc.apvtsPiano.getRawParameterValue ("midiChannel")
                                      : 1,
                                  midi, 96);
}

bool PianoPianoRollComponent::isBlackKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void PianoPianoRollComponent::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (full);
    
    auto& engine = proc.pianoEngine;
    int low = 60, high = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, low, high);
    const int nRows = juce::jmax (1, high - low + 1);
    const float rowH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);
    
    const juce::String names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int scaleIdx = proc.apvtsMain.getRawParameterValue ("scale") != nullptr
                             ? (int) proc.apvtsMain.getRawParameterValue ("scale")->load()
                             : 0;
    const int rootPc = proc.apvtsMain.getRawParameterValue ("rootNote") != nullptr
                           ? (int) proc.apvtsMain.getRawParameterValue ("rootNote")->load()
                           : 0;

    // Draw keys
    for (int m = high; m >= low; --m)
    {
        int idx = high - m;
        float y = full.getY() + (float) idx * rowH;
        auto row = juce::Rectangle<float> (full.getX(), y, full.getWidth(), rowH);

        bool bk = isBlackKey (m);
        const bool inScale = bridge::pitchClassInPresetScale (scaleIdx, rootPc, m % 12);
        juce::Colour fill = bk ? bridge::hig::systemBackground : bridge::hig::secondaryLabel;
        if (! inScale)
            fill = fill.interpolatedWith (bridge::hig::tertiaryGroupedBackground, 0.55f);
        g.setColour (fill);
        g.fillRect (row.reduced(0, 0.5f));

        g.setColour (bridge::hig::separatorOpaque.withAlpha (0.55f));
        g.drawHorizontalLine ((int)(y + rowH), full.getX(), full.getRight());

        g.setColour ((bk ? bridge::hig::tertiaryLabel : bridge::hig::secondaryLabel)
                         .interpolatedWith (bridge::hig::quaternaryLabel, inScale ? 0.0f : 0.65f));
        g.setFont (bridge::hig::uiFont (10.0f));
        g.drawText (names[m % 12] + juce::String (m / 12 - 1),
                    row.reduced(2.0f, 0.0f).removeFromRight(full.getWidth() * 0.7f),
                    juce::Justification::centredRight, true);
    }
}

// ─── PianoGridComponent ────────────────────────────────────────────────────────

PianoGridComponent::PianoGridComponent (PianoPanel& panel, BridgeProcessor& p)
    : parentPanel (panel), proc (p)
{}

void PianoGridComponent::setCellSize (float w, float h)
{
    storedCellW = w;
    storedCellH = h;
}

void PianoGridComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        if (e.mods.isShiftDown())
            parentPanel.adjustZoomX (wheel.deltaY);
        else
            parentPanel.adjustZoomY (wheel.deltaY);
    }
    else if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        vp->mouseWheelMove (e.getEventRelativeTo (vp), wheel);
    }
}

void PianoGridComponent::paint (juce::Graphics& g)
{
    auto& engine  = proc.pianoEngine;
    auto& pattern = engine.getPatternForGrid();

    int loopStart = 1, loopEnd = PianoPreset::NUM_STEPS;
    proc.getPianoLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillAll();

    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = PianoPreset::NUM_STEPS;

    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    const float cellH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);

    const int scaleIdx = proc.apvtsMain.getRawParameterValue ("scale") != nullptr
                             ? (int) proc.apvtsMain.getRawParameterValue ("scale")->load()
                             : 0;
    const int rootPc = proc.apvtsMain.getRawParameterValue ("rootNote") != nullptr
                           ? (int) proc.apvtsMain.getRawParameterValue ("rootNote")->load()
                           : 0;
    for (int row = 0; row < nRows; ++row)
    {
        const int midi = maxMidi - row;
        const bool inScale = bridge::pitchClassInPresetScale (scaleIdx, rootPc, midi % 12);
        const float cy = (float) row * cellH;
        g.setColour (inScale ? juce::Colours::white.withAlpha (0.04f) : juce::Colours::black.withAlpha (0.055f));
        g.fillRect (0.0f, cy, full.getWidth(), cellH);
    }

    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.4f));
    for (int row = 0; row < nRows; ++row) {
        float cy = (float) row * cellH;
        g.drawHorizontalLine ((int)cy, 0.0f, full.getWidth());
    }
    for (int step = 0; step < nSteps; ++step) {
        float cx = (float) step * cellW;
        bool isBeat = (step % 4 == 0);
        g.setColour (isBeat ? bridge::hig::quaternaryFill : bridge::hig::separatorOpaque.withAlpha (0.45f));
        g.drawVerticalLine ((int)cx, 0.0f, full.getHeight());
    }

    // Dim out-of-loop areas
    if (ls0 > 0) {
        g.setColour (juce::Colours::black.withAlpha (0.52f));
        g.fillRect (0.0f, 0.0f, ls0 * cellW, full.getHeight());
    }
    if (le0 < nSteps - 1) {
        g.setColour (juce::Colours::black.withAlpha (0.52f));
        g.fillRect ((le0 + 1) * cellW, 0.0f, (nSteps - le0 - 1) * cellW, full.getHeight());
    }

    // Draw Active Notes (Capsules)
    juce::Colour accent = bridge::colors::accentPiano();
    for (int step = 0; step < nSteps; ++step)
    {
        const auto& hit = pattern[(size_t) step];
        if (! hit.active) continue;

        const int midi = juce::jlimit (minMidi, maxMidi, hit.midiNote);
        int displayRow = maxMidi - midi;
        displayRow = juce::jlimit (0, nRows - 1, displayRow);

        float cx = (float) step * cellW;
        float cy = (float) displayRow * cellH;
        auto cell = juce::Rectangle<float> (cx, cy, cellW, cellH);

        juce::Colour col = accent.withAlpha (hit.isGhost ? 0.35f : 0.85f);
        float velFrac = hit.velocity / 127.0f;
        if (hit.isGhost)
            col = col.withAlpha (0.4f + velFrac * 0.2f);
        if (step == currentStep)
            col = col.brighter (0.4f);

        // Natively span the cell width. We reduce Y to make them pill-like.
        auto block = cell.reduced(1.0f, cellH * 0.2f);
        
        g.setColour (col);
        g.fillRect (block);
    }

    // Playhead
    if (currentStep >= 0 && currentStep < nSteps)
    {
        float cx = (float) currentStep * cellW;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect  (cx, 0.0f, cellW, full.getHeight());
        
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect  (cx, 0.0f, 2.0f, full.getHeight());
    }
}

void PianoGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = PianoPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    const float cellH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;
    
    dragOriginStep = step;
    
    // Allow standard left click to create/move
    auto& pat = const_cast<PianoPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;
    
    if (! pat[(size_t)step].active || pat[(size_t)step].midiNote != targetMidi)
    {
        const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
        pat[(size_t)step].active   = true;
        pat[(size_t)step].degree   = deg;
        pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
        pat[(size_t)step].velocity = 100;
        pat[(size_t)step].isGhost  = false;
    }
    proc.rebuildPianoGridPreview();
    repaint();
}


void PianoGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragOriginStep < 0) return;
    
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = PianoPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    const float cellH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;

    if (step != dragOriginStep)
    {
        auto& pat = const_cast<PianoPattern&>(engine.getPattern());
        // Move note securely
        if (pat[(size_t)dragOriginStep].active)
        {
            pat[(size_t)step] = pat[(size_t)dragOriginStep];
            pat[(size_t)dragOriginStep].active = false;
        }
        dragOriginStep = step; // update anchor
    }
    
    // Update pitch if row changed
    auto& pat = const_cast<PianoPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;
    
    if (pat[(size_t)step].active && pat[(size_t)step].midiNote != targetMidi)
    {
        const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
        pat[(size_t)step].degree   = deg;
        pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
    }

    proc.rebuildPianoGridPreview();
    repaint();
}

void PianoGridComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    const int nSteps = PianoPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    int step = (int)(e.x / cellW);
    
    if (step >= 0 && step < nSteps)
    {
        auto& pat = const_cast<PianoPattern&>(engine.getPattern());
        pat[(size_t)step].active = false;
        proc.rebuildPianoGridPreview();
        repaint();
    }
}
void PianoGridComponent::resized() {}

void PianoGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── Refactored ────────────────────────────────────────────────────────────

PianoPanel::PianoPanel (BridgeProcessor& p)
    : proc (p),
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
    proc.apvtsMain.addParameterListener ("scale", this);
    proc.apvtsMain.addParameterListener ("rootNote", this);
    proc.apvtsMain.addParameterListener ("octave", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity" })
        proc.apvtsPiano.addParameterListener (id, this);

    addAndMakeVisible (melodicViewport);
    melodicViewport.setViewedComponent (&melodicBody, false);
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

    proc.rebuildPianoGridPreview();
    stepTimer.startTimerHz (30);
    applyPianoPageState();
}

PianoPanel::~PianoPanel()
{
    melodicViewport.getVerticalScrollBar().setLookAndFeel (nullptr);
    proc.apvtsMain.removeParameterListener ("pianoOn", this);
    proc.apvtsMain.removeParameterListener ("scale", this);
    proc.apvtsMain.removeParameterListener ("rootNote", this);
    proc.apvtsMain.removeParameterListener ("octave", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsPiano.removeParameterListener ("tickerSpeed", this);
    proc.apvtsPiano.removeParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity" })
        proc.apvtsPiano.removeParameterListener (id, this);
    proc.apvtsPiano.state.removeListener (this);
}

void PianoPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, PianoPreset::NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void PianoPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    deferredPianoGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

void PianoPanel::applyPianoPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("pianoOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("pianoOn")->load() > 0.5f;

    bottomHalf.setEnabled (on);
    instrumentStrip.setTrackPowered (on);
    melodicViewport.setEnabled (on);
    melodicBody.roll.setEnabled (on);
    melodicBody.grid.setEnabled (on);
    loopStrip.setEnabled (on);

    const float dim = on ? 1.0f : 0.42f;
    melodicViewport.setAlpha (dim);
    melodicBody.roll.setAlpha (dim);
    melodicBody.grid.setAlpha (dim);
    loopStrip.setAlpha (dim);
    bottomHalf.setAlpha (dim);
}

void PianoPanel::fitPatternInView()
{
    zoomX = 1.0f;
    zoomY = 1.0f;
    resized();
    melodicViewport.setViewPosition (0, 0);
}

void PianoPanel::adjustZoomX (float wheelDeltaY)
{
    const int oldX = melodicViewport.getViewPositionX();
    const int oldY = melodicViewport.getViewPositionY();
    auto* body     = melodicViewport.getViewedComponent();
    const int oldBw = body != nullptr ? body->getWidth() : 1;
    const int oldBh = body != nullptr ? body->getHeight() : 1;

    zoomX = juce::jlimit (0.5f, 8.0f, zoomX * (1.0f + wheelDeltaY * 0.25f));
    resized();
    bridge::melodicViewportPreserveCentreAfterBodyResize (melodicViewport, oldX, oldY, oldBw, oldBh);
}

void PianoPanel::adjustZoomY (float wheelDeltaY)
{
    const int oldX = melodicViewport.getViewPositionX();
    const int oldY = melodicViewport.getViewPositionY();
    auto* body     = melodicViewport.getViewedComponent();
    const int oldBw = body != nullptr ? body->getWidth() : 1;
    const int oldBh = body != nullptr ? body->getHeight() : 1;

    zoomY = juce::jlimit (0.5f, 8.0f, zoomY * (1.0f + wheelDeltaY * 0.25f));
    resized();
    bridge::melodicViewportPreserveCentreAfterBodyResize (melodicViewport, oldX, oldY, oldBw, oldBh);
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

    melodicViewport.setBounds (card);
    const int viewW = juce::jmax (1, melodicViewport.getMaximumVisibleWidth());
    const int viewH = juce::jmax (1, melodicViewport.getHeight());

    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (proc.pianoEngine, minMidi, maxMidi);
    const int nRows  = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = PianoPreset::NUM_STEPS;
    const int strip  = (int) bridge::kMelodicKeyStripWidth;

    const float baseCellW = (float) (viewW - strip) / (float) juce::jmax (1, nSteps);
    const float baseCellH = (float) viewH / (float) nRows;
    const float cellW     = baseCellW * zoomX;
    const float cellH     = baseCellH * zoomY;

    const int bodyW = strip + (int) std::lround (cellW * (float) nSteps);
    const int bodyH = juce::jmax (1, (int) std::lround (cellH * (float) nRows));

    melodicBody.setMelodicCellSize (cellW, cellH);
    melodicBody.setSize (bodyW, bodyH);

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
    melodicBody.roll.repaint();
    melodicBody.grid.repaint();
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
    if (parameterID == "loopStart" || parameterID == "loopEnd" || parameterID == "playbackLoopOn"
        || parameterID == "scale" || parameterID == "rootNote" || parameterID == "octave"
        || parameterID == "uiTheme")
    {
        loopStrip.repaint();
        melodicBody.grid.repaint();
        melodicBody.roll.repaint();
        resized();
    }

    if (parameterID == "density" || parameterID == "complexity")
    {
        proc.syncPianoEngineFromAPVTS();
        int ls = 1, le = PianoPreset::NUM_STEPS;
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
        melodicBody.grid.update (step);
    }
}
