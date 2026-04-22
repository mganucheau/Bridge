#include "DrumsPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
#include "drums/DrumsStylePresets.h"
#include "drums/DrumsLookAndFeel.h"

namespace
{
/** Match melodic grid: lane column + step columns use same cellW / rowH as Bass/Piano/Guitar grid. */
void computeDrumGridGeometry (juce::Rectangle<float> fullBounds,
                              float& laneX, float& laneW,
                              float& originX, float& originY,
                              float& cellW, float& rowH)
{
    laneX = 0.0f;
    laneW = (float) bridge::kMelodicKeyStripWidth;
    const float gridLeft = laneW;
    const float gridW = juce::jmax (1.0f, fullBounds.getWidth() - laneW);
    const float gridBodyH = fullBounds.getHeight();
    cellW = gridW / (float) NUM_STEPS;
    rowH = gridBodyH / (float) bridge::kMelodicOctaveRows;
    originX = gridLeft;
    const float drumBlockH = rowH * (float) NUM_DRUMS;
    originY = (gridBodyH - drumBlockH) * 0.5f;
}
} // namespace

// ─── DrumGridComponent ────────────────────────────────────────────────────────

int DrumGridComponent::visualRowToDrum (int visualRow)
{
    visualRow = juce::jlimit (0, NUM_DRUMS - 1, visualRow);
    return NUM_DRUMS - 1 - visualRow;
}

void DrumGridComponent::syncGeometryFromBounds()
{
    const auto full = getLocalBounds().toFloat();
    if (full.getWidth() < 1.0f || full.getHeight() < 1.0f)
        return;
    computeDrumGridGeometry (full, geomLaneX, geomLaneW, geomOriginX, geomOriginY, geomCellW, geomRowH);
}

DrumGridComponent::DrumGridComponent (BridgeProcessor& p)
    : proc (p)
{
    auto styleLaneMute = [] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (0);
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack());
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffd5c4));
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim());
        btn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffff453a));
    };
    auto styleLaneSolo = [] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (0);
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack());
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb3d7ff));
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim());
        btn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff0a84ff));
    };

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        auto* mute = muteButtons.add (new juce::TextButton ("M"));
        mute->setTooltip ("Mute lane");
        styleLaneMute (*mute);
        addAndMakeVisible (mute);
        muteAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsDrums,
                                                                                       "mute_" + juce::String (drum),
                                                                                       *mute));

        auto* solo = soloButtons.add (new juce::TextButton ("S"));
        solo->setTooltip ("Solo lane (any solo mutes all others)");
        styleLaneSolo (*solo);
        addAndMakeVisible (solo);
        soloAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsDrums,
                                                                                       "solo_" + juce::String (drum),
                                                                                       *solo));
    }
}

void DrumGridComponent::paint (juce::Graphics& g)
{
    using namespace DrumsHIG;

    auto& pattern = proc.getPatternForGrid();
    int   patLen  = proc.drumEngine.getPatternLen();

    int loopStart = 1, loopEnd = NUM_STEPS;
    proc.getDrumsLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    if (geomLaneW <= 0.0f)
        syncGeometryFromBounds();

    g.setColour (bridge::colors::cardSurface());
    g.fillRect (full);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRect (full.reduced (0.5f), 1.0f);

    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (geomLaneX, full.getY(), geomLaneW, full.getHeight());
    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.5f));
    g.drawVerticalLine ((int) geomOriginX, (int) full.getY(), (int) full.getBottom());

    const float pad = 2.0f;

    g.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        const float cy = geomOriginY + (float) visualRow * geomRowH;
        const int btn = juce::jlimit (12, 17, (int) (geomRowH * 0.42f));
        const int pairW = btn * 2 + 4;
        auto laneRow = juce::Rectangle<float> (geomLaneX + 3.0f, cy, geomLaneW - 6.0f, geomRowH);
        auto nameArea = laneRow.removeFromLeft (juce::jmax (18.0f, laneRow.getWidth() - (float) pairW));
        g.setColour (juce::Colours::white);
        g.drawText (DRUM_NAMES[drum], nameArea.toNearestInt(), juce::Justification::centredLeft, true);
    }

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        float cy = geomOriginY + (float) visualRow * geomRowH;

        const bool muted = proc.apvtsDrums.getRawParameterValue ("mute_" + juce::String (drum))->load() > 0.5f;
        const bool soloed = proc.apvtsDrums.getRawParameterValue ("solo_" + juce::String (drum))->load() > 0.5f;
        bool anySolo = false;
        for (int d = 0; d < NUM_DRUMS; ++d)
            anySolo = anySolo || (proc.apvtsDrums.getRawParameterValue ("solo_" + juce::String (d))->load() > 0.5f);

        const bool audible = anySolo ? soloed : ! muted;

        for (int step = 0; step < NUM_STEPS; ++step)
        {
            float cx = geomOriginX + (float) step * geomCellW + pad;
            auto  cell = juce::Rectangle<float> (cx, cy + pad,
                                                 geomCellW - pad * 2.0f, geomRowH - pad * 2.0f);

            bool inPattern = (step < patLen);
            bool inLoop = inPattern && (step >= ls0 && step <= le0);
            const auto& hit = pattern[(size_t) step][(size_t) drum];

            if (! inPattern)
            {
                g.setColour (surfaceDim.interpolatedWith (surfaceContainerHigh, 0.35f));
                g.fillRect (cell);
                continue;
            }

            bool isBeat = (step % 4 == 0);

            if (! inLoop)
            {
                auto baseDim = surfaceDim.interpolatedWith (surfaceContainerHigh, 0.5f);
                if (hit.active)
                {
                    float velFrac = hit.velocity / 127.0f;
                    auto  col = DrumsColors::DrumColors[drum].withAlpha (0.22f + velFrac * 0.25f);
                    g.setColour (col);
                    g.fillRect (cell);
                }
                else
                {
                    g.setColour (baseDim);
                    g.fillRect (cell);
                }
                g.setColour (outlineVariant.withAlpha (0.25f));
                g.drawRect (cell, 1.0f);
                continue;
            }

            if (hit.active)
            {
                float velFrac = hit.velocity / 127.0f;
                auto  col = DrumsColors::DrumColors[drum];

                if (velFrac < 0.4f)
                    col = col.withAlpha (0.35f + velFrac * 0.6f);

                if (step == currentStep)
                    col = col.brighter (0.25f);

                if (! audible)
                    col = col.withAlpha (0.15f);

                g.setColour (col);
                g.fillRect (cell);

                float velLineW = cell.getWidth() * velFrac;
                g.setColour (onSurface.withAlpha (0.35f));
                g.fillRect  (cell.getX(), cell.getY(), velLineW, 2.0f);
            }
            else
            {
                auto baseCol = isBeat ? surfaceContainerHighest : surfaceContainerHigh;
                if (step == currentStep)
                    baseCol = primary().withAlpha (0.12f);

                if (! audible)
                    baseCol = baseCol.withAlpha (0.35f);

                g.setColour (baseCol);
                g.fillRect (cell);
            }

            auto borderCol = isBeat ? outline.withAlpha (0.55f) : outlineVariant.withAlpha (0.45f);
            g.setColour (borderCol);
            g.drawRect (cell, 1.0f);
        }
    }

    const bool drumsOn = proc.apvtsMain.getRawParameterValue ("drumsOn") != nullptr
                             && proc.apvtsMain.getRawParameterValue ("drumsOn")->load() > 0.5f;

    if (drumsOn && currentStep >= 0 && currentStep < NUM_STEPS)
    {
        float cx = geomOriginX + (float) currentStep * geomCellW;
        g.setColour (primary().withAlpha (0.08f));
        g.fillRect  (cx, geomOriginY, geomCellW, geomRowH * (float) NUM_DRUMS);

        g.setColour (primary().withAlpha (0.85f));
        g.fillRect  (cx + pad, geomOriginY, geomCellW - pad * 2.0f, 2.0f);
    }
}

void DrumGridComponent::mouseDown (const juce::MouseEvent& e)
{
    if (proc.apvtsMain.getRawParameterValue ("drumsOn") == nullptr
        || proc.apvtsMain.getRawParameterValue ("drumsOn")->load() <= 0.5f)
        return;

    int patLen = proc.drumEngine.getPatternLen();

    if (geomLaneW <= 0.0f)
        syncGeometryFromBounds();

    if (e.position.x < geomOriginX) return;

    int step = (int) ((e.position.x - geomOriginX) / geomCellW);
    int visualRow = (int) ((e.position.y - geomOriginY) / geomRowH);
    int drum = visualRowToDrum (visualRow);

    if (step < 0 || step >= patLen || visualRow < 0 || visualRow >= NUM_DRUMS
        || drum < 0 || drum >= NUM_DRUMS) return;

    auto& gridPat = const_cast<DrumPattern&> (proc.getPatternForGrid());
    gridPat[(size_t) step][(size_t) drum].active   = ! gridPat[(size_t) step][(size_t) drum].active;
    gridPat[(size_t) step][(size_t) drum].velocity = 100;

    repaint();
}

void DrumGridComponent::resized()
{
    syncGeometryFromBounds();

    const int rowHi = juce::jmax (1, (int) geomRowH);
    const int btn = juce::jlimit (12, 17, rowHi * 7 / 16);
    const int pairGap = 4;

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        const int y = (int) (geomOriginY + (float) visualRow * geomRowH);
        auto rowR = juce::Rectangle<int> ((int) geomLaneX, y, (int) geomLaneW, rowHi).reduced (3, 1);
        auto pair = rowR.removeFromRight (btn * 2 + pairGap);
        auto muteR = pair.removeFromLeft (btn).withSizeKeepingCentre (btn, btn);
        pair.removeFromLeft (pairGap);
        auto soloR = pair.withSizeKeepingCentre (btn, btn);
        muteButtons[drum]->setBounds (muteR);
        soloButtons[drum]->setBounds (soloR);
    }
}

void DrumGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── DrumsPanel ────────────────────────────────────────────────────────────────

DrumsPanel::DrumsPanel (BridgeProcessor& p)
    : proc (p),
      bottomHalf (p.apvtsDrums, p.apvtsMain, laf, bridge::colors::accentDrums(),
                  [this] { proc.triggerDrumsGenerate(); },
                  [this] (bool active) { proc.drumEngine.setFillHoldActive (active); }),
      instrumentStrip (InstrumentControlBar::makeDrumsConfig (p)),
      drumGrid (p)
{
    setLookAndFeel (&laf);

    proc.apvtsMain.addParameterListener ("loopStart", this);
    proc.apvtsMain.addParameterListener ("loopEnd", this);
    proc.apvtsMain.addParameterListener ("playbackLoopOn", this);
    proc.apvtsDrums.addParameterListener ("tickerSpeed", this);
    proc.apvtsDrums.addParameterListener ("style", this);
    proc.apvtsDrums.state.addListener (this);

    proc.apvtsMain.addParameterListener ("drumsOn", this);

    addAndMakeVisible (drumGrid);
    addAndMakeVisible (loopStrip);
    loopStrip.setStepLabelGutter ((int) bridge::kMelodicKeyStripWidth);
    addAndMakeVisible (bottomHalf);
    addAndMakeVisible (instrumentStrip);

    instrumentStrip.getMuteButton().setTooltip ("Mute Drums");
    instrumentStrip.getSoloButton().setTooltip ("Solo Drums");
    instrumentStrip.getMuteButton().onClick = [this]
    {
        const bool muted = instrumentStrip.getMuteButton().getToggleState();
        if (auto* par = proc.apvtsMain.getParameter ("drumsOn"))
            par->setValueNotifyingHost (muted ? 0.0f : 1.0f);
    };

    proc.rebuildDrumsGridPreview();
    stepTimer.startTimerHz (30);
    applyDrumsPageState();
}

DrumsPanel::~DrumsPanel()
{
    proc.apvtsMain.removeParameterListener ("drumsOn", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsDrums.removeParameterListener ("tickerSpeed", this);
    proc.apvtsDrums.removeParameterListener ("style", this);
    proc.apvtsDrums.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void DrumsPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value)
{
    value = juce::jlimit (1, NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void DrumsPanel::applyDrumsPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("drumsOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("drumsOn")->load() > 0.5f;

    bottomHalf.setEnabled (on);
    instrumentStrip.setTrackPowered (on);
    drumGrid.setEnabled (on);
    loopStrip.setEnabled (on);

    const float dim = on ? 1.0f : 0.42f;
    drumGrid.setAlpha (dim);
    loopStrip.setAlpha (dim);
    bottomHalf.setAlpha (dim);
}

void DrumsPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildDrumsGridPreview();
    triggerAsyncUpdate();
}

void DrumsPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto bounds = getLocalBounds();
    auto stripRow = bounds.removeFromTop (kDropdownH);
    instrumentStrip.setBounds (stripRow);
    auto inner = bounds.reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 0);

    auto card = shell.mainCard.reduced (8, 8);
    loopStrip.setBounds (card.removeFromTop ((int) bridge::kLoopRangeStripHeightPx).reduced (4, 0));
    loopStrip.setAccent (bridge::colors::accentDrums());
    drumGrid.setBounds (card);

    bottomHalf.setBounds (shell.bottomStrip);
}

void DrumsPanel::paint (juce::Graphics& g)
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

void DrumsPanel::handleAsyncUpdate()
{
    drumGrid.repaint();
    loopStrip.repaint();
}

void DrumsPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "drumsOn")
    {
        applyDrumsPageState();
        return;
    }
    if (parameterID == "loopStart" || parameterID == "loopEnd" || parameterID == "playbackLoopOn"
        || parameterID == "uiTheme")
    {
        loopStrip.repaint();
        drumGrid.repaint();
    }

    proc.rebuildDrumsGridPreview();
    triggerAsyncUpdate();
}

void DrumsPanel::updateStepAnimation()
{
    const int step = proc.drumsCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        drumGrid.update (step);
    }
}
