#include "DrumsPanel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "MelodicGridLayout.h"
#include "drums/DrumsStylePresets.h"
#include "drums/DrumsLookAndFeel.h"

namespace
{
constexpr float kDrumGridMargin   = 12.0f;
constexpr float kDrumGridHeaderH  = 22.0f;
constexpr float kDrumLabelColW    = 60.0f;
constexpr float kDrumMSColW       = 50.0f;
constexpr float kDrumGapAfterMS   = 8.0f;

void computeDrumGridGeometry (juce::Rectangle<float> fullBounds,
                              float& innerX, float& innerY, float& innerW, float& innerH,
                              float& gridTop, float& hBody,
                              float& originX, float& originY, float& cellSize,
                              float& labelX, float& msX, float& leftBlockW)
{
    auto inner = fullBounds.reduced (kDrumGridMargin);
    innerX = inner.getX();
    innerY = inner.getY();
    innerW = inner.getWidth();
    innerH = inner.getHeight();
    gridTop = innerY + kDrumGridHeaderH;
    hBody = innerH - kDrumGridHeaderH;

    leftBlockW = kDrumLabelColW + kDrumMSColW + kDrumGapAfterMS;
    const float gridInnerX = innerX + leftBlockW;
    const float gridInnerW = innerW - leftBlockW;
    bridge::computeSquareMelodicGrid (gridInnerX, gridInnerW, gridTop, hBody,
                                      NUM_STEPS, NUM_DRUMS, originX, originY, cellSize);
    labelX = innerX;
    msX    = innerX + kDrumLabelColW;
}
} // namespace

// ─── DrumGridComponent ────────────────────────────────────────────────────────

int DrumGridComponent::visualRowToDrum (int visualRow)
{
    return juce::jlimit (0, NUM_DRUMS - 1, visualRow);
}

DrumGridComponent::DrumGridComponent (BridgeProcessor& p)
    : proc (p)
{
    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        auto* mute = muteButtons.add (new juce::ToggleButton ("M"));
        mute->setTooltip ("Mute lane");
        addAndMakeVisible (mute);
        muteAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsDrums,
                                                                                       "mute_" + juce::String (drum),
                                                                                       *mute));

        auto* solo = soloButtons.add (new juce::ToggleButton ("S"));
        solo->setTooltip ("Solo lane (any solo mutes all others)");
        addAndMakeVisible (solo);
        soloAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsDrums,
                                                                                       "solo_" + juce::String (drum),
                                                                                       *solo));
    }
}

void DrumGridComponent::paint (juce::Graphics& g)
{
    using namespace DrumsM3;

    auto& pattern = proc.getPatternForGrid();
    int   patLen  = proc.drumEngine.getPatternLen();

    int loopStart = 1, loopEnd = NUM_STEPS;
    proc.getDrumsLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    drawShadow (g, full, cornerLarge, 1);
    fillSurface (g, full, surfaceContainer, cornerLarge);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRoundedRectangle (full.reduced (0.5f), cornerLarge, 1.0f);

    float innerX = 0, innerY = 0, innerW = 0, innerH = 0;
    float gridTop = 0, hBody = 0;
    float originX = 0, originY = 0, cellSize = 0;
    float labelX = 0, msX = 0, leftBlockW = 0;

    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, labelX, msX, leftBlockW);

    const float pad = 2.0f;

    g.setColour (onSurfaceVariant.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        float cx = originX + (float) step * cellSize;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, innerY, cellSize, kDrumGridHeaderH - 2.0f),
                    juce::Justification::centred, false);
    }

    g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        const float cy = originY + (float) visualRow * cellSize;
        auto labelRow = juce::Rectangle<float> (labelX, cy, kDrumLabelColW - 6.0f, cellSize);
        g.setColour (DrumsColors::TextPrimary.withAlpha (0.92f));
        g.drawText (DRUM_NAMES[drum], labelRow, juce::Justification::centredLeft, false);
    }

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        float cy = originY + (float) visualRow * cellSize;

        const bool muted = proc.apvtsDrums.getRawParameterValue ("mute_" + juce::String (drum))->load() > 0.5f;
        const bool soloed = proc.apvtsDrums.getRawParameterValue ("solo_" + juce::String (drum))->load() > 0.5f;
        bool anySolo = false;
        for (int d = 0; d < NUM_DRUMS; ++d)
            anySolo = anySolo || (proc.apvtsDrums.getRawParameterValue ("solo_" + juce::String (d))->load() > 0.5f);

        const bool audible = anySolo ? soloed : ! muted;

        for (int step = 0; step < NUM_STEPS; ++step)
        {
            float cx = originX + (float) step * cellSize + pad;
            auto  cell = juce::Rectangle<float> (cx, cy + pad,
                                                 cellSize - pad * 2.0f, cellSize - pad * 2.0f);

            bool inPattern = (step < patLen);
            bool inLoop = inPattern && (step >= ls0 && step <= le0);
            const auto& hit = pattern[(size_t) step][(size_t) drum];

            if (! inPattern)
            {
                g.setColour (surfaceDim.interpolatedWith (surfaceContainerHigh, 0.35f));
                g.fillRoundedRectangle (cell, cornerExtraSmall);
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
                    g.fillRoundedRectangle (cell, cornerExtraSmall);
                }
                else
                {
                    g.setColour (baseDim);
                    g.fillRoundedRectangle (cell, cornerExtraSmall);
                }
                g.setColour (outlineVariant.withAlpha (0.25f));
                g.drawRoundedRectangle (cell, cornerExtraSmall, 1.0f);
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
                g.fillRoundedRectangle (cell, cornerExtraSmall);

                float velLineW = cell.getWidth() * velFrac;
                g.setColour (onSurface.withAlpha (0.35f));
                g.fillRect  (cell.getX(), cell.getY(), velLineW, 2.0f);
            }
            else
            {
                auto baseCol = isBeat ? surfaceContainerHighest : surfaceContainerHigh;
                if (step == currentStep)
                    baseCol = primary.withAlpha (0.12f);

                if (! audible)
                    baseCol = baseCol.withAlpha (0.35f);

                g.setColour (baseCol);
                g.fillRoundedRectangle (cell, cornerExtraSmall);
            }

            auto borderCol = isBeat ? outline.withAlpha (0.55f) : outlineVariant.withAlpha (0.45f);
            g.setColour (borderCol);
            g.drawRoundedRectangle (cell, cornerExtraSmall, 1.0f);
        }
    }

    const bool drumsOn = proc.apvtsMain.getRawParameterValue ("drumsOn") != nullptr
                             && proc.apvtsMain.getRawParameterValue ("drumsOn")->load() > 0.5f;

    if (drumsOn && currentStep >= 0 && currentStep < NUM_STEPS)
    {
        float cx = originX + (float) currentStep * cellSize;
        g.setColour (primary.withAlpha (0.08f));
        g.fillRect  (cx, originY, cellSize, cellSize * (float) NUM_DRUMS);

        g.setColour (primary.withAlpha (0.85f));
        g.fillRect  (cx + pad, innerY, cellSize - pad * 2.0f, 2.0f);
    }
}

void DrumGridComponent::mouseDown (const juce::MouseEvent& e)
{
    if (proc.apvtsMain.getRawParameterValue ("drumsOn") == nullptr
        || proc.apvtsMain.getRawParameterValue ("drumsOn")->load() <= 0.5f)
        return;

    int patLen = proc.drumEngine.getPatternLen();

    auto full = getLocalBounds().toFloat();
    float innerX = 0, innerY = 0, innerW = 0, innerH = 0;
    float gridTop = 0, hBody = 0;
    float originX = 0, originY = 0, cellSize = 0;
    float labelX = 0, msX = 0, leftBlockW = 0;
    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, labelX, msX, leftBlockW);

    if (e.position.y < gridTop || e.position.x < originX) return;

    int step = (int) ((e.position.x - originX) / cellSize);
    int visualRow = (int) ((e.position.y - originY) / cellSize);
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
    constexpr int buttonW = 20;
    constexpr int buttonGap = 4;

    auto full = getLocalBounds().toFloat();
    float innerX = 0, innerY = 0, innerW = 0, innerH = 0;
    float gridTop = 0, hBody = 0;
    float originX = 0, originY = 0, cellSize = 0;
    float labelX = 0, msX = 0, leftBlockW = 0;
    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, labelX, msX, leftBlockW);

    const int rowH = (int) juce::jmax (1.0f, cellSize);

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        int y = (int) (originY + (float) visualRow * cellSize);
        auto row = juce::Rectangle<int> ((int) msX, y,
                                         (int) kDrumMSColW, rowH);
        const int yPad = juce::jmax (0, (rowH - buttonW) / 2);
        auto controls = row.reduced (0, yPad);
        auto mute = controls.removeFromLeft (buttonW);
        controls.removeFromLeft (buttonGap);
        auto solo = controls.removeFromLeft (buttonW);

        muteButtons[drum]->setBounds (mute);
        soloButtons[drum]->setBounds (solo);
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
      bottomHalf (p.apvtsDrums, laf, bridge::colors::accentDrums,
                  [this] { proc.triggerDrumsGenerate(); },
                  [this] (bool active) { proc.drumEngine.setFillHoldActive (active); }),
      drumGrid (p)
{
    setLookAndFeel (&laf);

    proc.apvtsDrums.addParameterListener ("loopStart", this);
    proc.apvtsDrums.addParameterListener ("loopEnd", this);
    proc.apvtsDrums.addParameterListener ("loopOn", this);
    proc.apvtsDrums.addParameterListener ("tickerSpeed", this);
    proc.apvtsDrums.addParameterListener ("style", this);
    proc.apvtsDrums.state.addListener (this);

    proc.apvtsMain.addParameterListener ("drumsOn", this);

    addAndMakeVisible (drumGrid);
    addAndMakeVisible (bottomHalf);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, bridge::colors::textDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsDrums, "style", styleBox);

    addAndMakeVisible (pagePower);
    powerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, "drumsOn", pagePower);

    proc.rebuildDrumsGridPreview();
    stepTimer.startTimerHz (30);
    applyDrumsPageState();
}

DrumsPanel::~DrumsPanel()
{
    proc.apvtsMain.removeParameterListener ("drumsOn", this);
    proc.apvtsDrums.removeParameterListener ("loopStart", this);
    proc.apvtsDrums.removeParameterListener ("loopEnd", this);
    proc.apvtsDrums.removeParameterListener ("loopOn", this);
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
    styleLabel.setEnabled (on);
    styleBox.setEnabled (on);
    drumGrid.setEnabled (on);

    pagePower.setEnabled (true);
    pagePower.setToggleState (on, juce::dontSendNotification);
    pagePower.setAlpha (1.0f);

    const float dim = on ? 1.0f : 0.42f;
    styleLabel.setAlpha (dim);
    styleBox.setAlpha (dim);
    drumGrid.setAlpha (dim);
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

    const auto inner = getLocalBounds().reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 42);

    auto dropdownRow = inner.withHeight (42);
    bridge::panelLayout::layoutDrumsStyleDropdownRow (dropdownRow, styleLabel, styleBox, pagePower);

    auto card = shell.mainCard.reduced (8, 8);
    drumGrid.setBounds (card);

    bottomHalf.setBounds (shell.knobsCard.getUnion (shell.loopActionsCard));
}

void DrumsPanel::paint (juce::Graphics& g)
{
    using namespace bridge::instrumentLayout;
    g.fillAll (bridge::colors::background);

    auto shell = bridge::panelLayout::splitInstrumentContent (getLocalBounds().reduced (16), 42);

    auto drawCard = [&] (juce::Rectangle<int> r)
    {
        auto rf = r.toFloat();
        g.setColour (bridge::colors::cardSurface);
        g.fillRoundedRectangle (rf, (float) kCardRadius);
        g.setColour (bridge::colors::cardOutline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), (float) kCardRadius, 1.0f);
    };
    drawCard (shell.mainCard);
}

void DrumsPanel::handleAsyncUpdate()
{
    drumGrid.repaint();
}

void DrumsPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "drumsOn")
    {
        applyDrumsPageState();
        return;
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
