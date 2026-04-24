#include "GuitarPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
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

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.guitarEngine.setFillHoldActive (true);
            proc.rebuildGuitarGridPreview();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.guitarEngine.setFillHoldActive (false);
            proc.rebuildGuitarGridPreview();
        }
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── GuitarMelodicBody ───────────────────────────────────────────────────────────

GuitarMelodicBody::GuitarMelodicBody (GuitarPanel& panelIn, BridgeProcessor& processor)
    : roll (processor), grid (panelIn, processor)
{
    addAndMakeVisible (roll);
    addAndMakeVisible (grid);
}

void GuitarMelodicBody::setMelodicCellSize (float cellW, float cellH)
{
    layoutCellW = cellW;
    layoutCellH = cellH;
}

void GuitarMelodicBody::resized()
{
    const int strip = (int) bridge::kMelodicKeyStripWidth;
    const int h       = juce::jmax (1, getHeight());
    const int gridW   = juce::jmax (1, getWidth() - strip);
    roll.setCellSize (layoutCellW, layoutCellH);
    grid.setCellSize (layoutCellW, layoutCellH);
    roll.setBounds (0, 0, strip, h);
    grid.setBounds (strip, 0, gridW, h);
}

// ─── GuitarPianoRollComponent ───────────────────────────────────────────────────────

GuitarPianoRollComponent::GuitarPianoRollComponent (BridgeProcessor& p) : proc (p) {}

void GuitarPianoRollComponent::setCellSize (float w, float h)
{
    storedCellW = w;
    storedCellH = h;
}

void GuitarPianoRollComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.guitarEngine;
    int low = 60, high = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, low, high);
    const int nRows = juce::jmax (1, high - low + 1);
    const float rowH = storedCellH > 0.01f ? storedCellH : ((float) getHeight() / (float) nRows);
    const int row = (int) (e.position.y / juce::jmax (1.0f, rowH));
    const int idx = juce::jlimit (0, nRows - 1, row);
    const int midi = high - idx;
    proc.queueMelodicPreviewNote (proc.apvtsGuitar.getRawParameterValue ("midiChannel") != nullptr
                                      ? (int) *proc.apvtsGuitar.getRawParameterValue ("midiChannel")
                                      : 1,
                                  midi, 96);
}

bool GuitarPianoRollComponent::isBlackKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void GuitarPianoRollComponent::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (full);
    
    auto& engine = proc.guitarEngine;
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

// ─── GuitarGridComponent ────────────────────────────────────────────────────────

GuitarGridComponent::GuitarGridComponent (GuitarPanel& panelIn, BridgeProcessor& p)
    : panel (panelIn), proc (p)
{}

void GuitarGridComponent::setCellSize (float w, float h)
{
    storedCellW = w;
    storedCellH = h;
}

void GuitarGridComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        panel.adjustZoomY (w.deltaY);
    else if (e.mods.isShiftDown())
        panel.adjustZoomX (w.deltaY);
    else if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        vp->mouseWheelMove (e.getEventRelativeTo (vp), w);
}

void GuitarGridComponent::magnify (const juce::MouseEvent&, float scaleFactor)
{
    panel.adjustZoomY (scaleFactor - 1.0f);
}

void GuitarGridComponent::paint (juce::Graphics& g)
{
    auto& engine  = proc.guitarEngine;
    auto& pattern = engine.getPatternForGrid();

    int loopStart = 1, loopEnd = GuitarPreset::NUM_STEPS;
    proc.getGuitarLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillAll();

    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = GuitarPreset::NUM_STEPS;

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
    juce::Colour accent = bridge::colors::accentGuitar();
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

void GuitarGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.guitarEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = GuitarPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    const float cellH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;
    
    dragOriginStep = step;
    
    // Allow standard left click to create/move
    auto& pat = const_cast<GuitarPattern&>(engine.getPattern());
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
    proc.rebuildGuitarGridPreview();
    repaint();
}


void GuitarGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragOriginStep < 0) return;
    
    auto& engine = proc.guitarEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (engine, minMidi, maxMidi);
    const int nRows = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = GuitarPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    const float cellH = storedCellH > 0.01f ? storedCellH : (full.getHeight() / (float) nRows);

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;

    if (step != dragOriginStep)
    {
        auto& pat = const_cast<GuitarPattern&>(engine.getPattern());
        // Move note securely
        if (pat[(size_t)dragOriginStep].active)
        {
            pat[(size_t)step] = pat[(size_t)dragOriginStep];
            pat[(size_t)dragOriginStep].active = false;
        }
        dragOriginStep = step; // update anchor
    }
    
    // Update pitch if row changed
    auto& pat = const_cast<GuitarPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;
    
    if (pat[(size_t)step].active && pat[(size_t)step].midiNote != targetMidi)
    {
        const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
        pat[(size_t)step].degree   = deg;
        pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
    }

    proc.rebuildGuitarGridPreview();
    repaint();
}

void GuitarGridComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& engine = proc.guitarEngine;
    auto full = getLocalBounds().toFloat();
    const int nSteps = GuitarPreset::NUM_STEPS;
    const float cellW = storedCellW > 0.01f ? storedCellW : (full.getWidth() / (float) nSteps);
    int step = (int)(e.x / cellW);
    
    if (step >= 0 && step < nSteps)
    {
        auto& pat = const_cast<GuitarPattern&>(engine.getPattern());
        pat[(size_t)step].active = false;
        proc.rebuildGuitarGridPreview();
        repaint();
    }
}
void GuitarGridComponent::resized() {}

void GuitarGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── Refactored ────────────────────────────────────────────────────────────

GuitarPanel::GuitarPanel (BridgeProcessor& p)
    : proc (p),
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
    proc.apvtsMain.addParameterListener ("scale", this);
    proc.apvtsMain.addParameterListener ("rootNote", this);
    proc.apvtsMain.addParameterListener ("octave", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity" })
        proc.apvtsGuitar.addParameterListener (id, this);

    addAndMakeVisible (melodicViewport);
    melodicViewport.setViewedComponent (&melodicBody, false);
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
            repaint();
        });
    };
    melodicTonalityPrev = std::move (storedPrev);

    proc.rebuildGuitarGridPreview();
    stepTimer.startTimerHz (30);
    applyGuitarPageState();
}

GuitarPanel::~GuitarPanel()
{
    proc.onMelodicTonalityChanged = std::move (melodicTonalityPrev);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (nullptr);
    proc.apvtsMain.removeParameterListener ("guitarOn", this);
    proc.apvtsMain.removeParameterListener ("scale", this);
    proc.apvtsMain.removeParameterListener ("rootNote", this);
    proc.apvtsMain.removeParameterListener ("octave", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsGuitar.removeParameterListener ("tickerSpeed", this);
    proc.apvtsGuitar.removeParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "hold", "velocity", "fillRate", "complexity",
                            "ghostAmount", "sustain", "temperature", "staccato", "intensity" })
        proc.apvtsGuitar.removeParameterListener (id, this);
    proc.apvtsGuitar.state.removeListener (this);
}

void GuitarPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, GuitarPreset::NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void GuitarPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    deferredGuitarGridPreviewRebuild = true;
    triggerAsyncUpdate();
}

void GuitarPanel::applyGuitarPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("guitarOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("guitarOn")->load() > 0.5f;

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

void GuitarPanel::scrollMelodicViewportToPatternCentre()
{
    const auto extent = bridge::getPatternMidiExtent (proc.guitarEngine);
    const int pMin = extent.first;
    const int pMax = extent.second;
    const int midMidi = (pMin <= pMax) ? (pMin + pMax) / 2
                                       : (bridge::kMelodicMinMidi + bridge::kMelodicMaxMidi) / 2;
    const int spanRows = bridge::kMelodicMaxMidi - bridge::kMelodicMinMidi + 1;
    const float rowH = (float) melodicBody.getHeight() / (float) spanRows;
    const int vpH = juce::jmax (1, melodicViewport.getHeight());
    const int bodyH = melodicBody.getHeight();
    const int scrollY = juce::jlimit (0, juce::jmax (0, bodyH - vpH),
                                      (int) ((bridge::kMelodicMaxMidi - midMidi) * rowH) - vpH / 2);
    melodicViewport.setViewPosition (melodicViewport.getViewPositionX(), scrollY);
}

void GuitarPanel::fitPatternInView()
{
    zoomX = 1.0f;
    zoomY = 1.0f;
    resized();
    scrollMelodicViewportToPatternCentre();
}

void GuitarPanel::adjustZoomX (float delta)
{
    zoomX = juce::jlimit (0.4f, 6.0f, zoomX * (1.0f + delta * 0.3f));
    resized();
}

void GuitarPanel::adjustZoomY (float delta)
{
    zoomY = juce::jlimit (0.4f, 6.0f, zoomY * (1.0f + delta * 0.3f));
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

    melodicViewport.setBounds (card);
    const int viewW = juce::jmax (1, melodicViewport.getWidth());
    const int viewH = juce::jmax (1, melodicViewport.getHeight());

    int minMidi = 60, maxMidi = 72;
    bridge::computeMelodicPitchWindowFromCommittedPattern (proc.guitarEngine, minMidi, maxMidi);
    const int nRows  = juce::jmax (1, maxMidi - minMidi + 1);
    const int nSteps = GuitarPreset::NUM_STEPS;
    const int strip  = (int) bridge::kMelodicKeyStripWidth;

    const float baseCellW = (float) (viewW - strip) / (float) juce::jmax (1, nSteps);
    const float baseCellH = (float) viewH / (float) nRows;
    const float cellW     = baseCellW * zoomX;
    const float cellH     = baseCellH * zoomY;

    const int bodyW = strip + (int) (cellW * (float) nSteps);
    const int bodyH = juce::jmax (1, (int) (cellH * (float) nRows));

    melodicBody.setMelodicCellSize (cellW, cellH);
    melodicBody.setSize (bodyW, bodyH);

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
    melodicBody.roll.repaint();
    melodicBody.grid.repaint();
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
        proc.syncGuitarEngineFromAPVTS();
        int ls = 1, le = GuitarPreset::NUM_STEPS;
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
        melodicBody.grid.update (step);
    }
}
