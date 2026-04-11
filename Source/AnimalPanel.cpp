#include "AnimalPanel.h"
#include "BridgeInstrumentStyles.h"
#include "MelodicGridLayout.h"
#include "bootsy/BootsyStylePresets.h"

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

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
            proc.drumEngine.setFillHoldActive (true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
            proc.drumEngine.setFillHoldActive (false);
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── DrumGridComponent ────────────────────────────────────────────────────────

int DrumGridComponent::visualRowToDrum (int visualRow)
{
    // Kick at top: visual row index maps directly onto the drum index.
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
        muteAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsAnimal,
                                                                                       "mute_" + juce::String (drum),
                                                                                       *mute));

        auto* solo = soloButtons.add (new juce::ToggleButton ("S"));
        solo->setTooltip ("Solo lane (any solo mutes all others)");
        addAndMakeVisible (solo);
        soloAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvtsAnimal,
                                                                                       "solo_" + juce::String (drum),
                                                                                       *solo));
    }
}

void DrumGridComponent::paint (juce::Graphics& g)
{
    using namespace AnimalM3;

    auto& pattern = proc.getPatternForGrid();
    int   patLen  = proc.drumEngine.getPatternLen();

    int loopStart = 1, loopEnd = NUM_STEPS;
    proc.getAnimalLoopBounds (loopStart, loopEnd);
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

    // Step numbers above the grid
    g.setColour (onSurfaceVariant.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        float cx = originX + (float) step * cellSize;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, innerY, cellSize, kDrumGridHeaderH - 2.0f),
                    juce::Justification::centred, false);
    }

    // Row labels (drum names)
    g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        const float cy = originY + (float) visualRow * cellSize;
        auto labelRow = juce::Rectangle<float> (labelX, cy, kDrumLabelColW - 6.0f, cellSize);
        g.setColour (AnimalColors::TextPrimary.withAlpha (0.92f));
        g.drawText (DRUM_NAMES[drum], labelRow, juce::Justification::centredLeft, false);
    }

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        float cy = originY + (float) visualRow * cellSize;

        const bool muted = proc.apvtsAnimal.getRawParameterValue ("mute_" + juce::String (drum))->load() > 0.5f;
        const bool soloed = proc.apvtsAnimal.getRawParameterValue ("solo_" + juce::String (drum))->load() > 0.5f;
        bool anySolo = false;
        for (int d = 0; d < NUM_DRUMS; ++d)
            anySolo = anySolo || (proc.apvtsAnimal.getRawParameterValue ("solo_" + juce::String (d))->load() > 0.5f);

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
                    auto  col = AnimalColors::DrumColors[drum].withAlpha (0.22f + velFrac * 0.25f);
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
                auto  col = AnimalColors::DrumColors[drum];

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

    if (currentStep >= 0 && currentStep < NUM_STEPS)
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
    int patLen = proc.drumEngine.getPatternLen();

    int loopStart = 1, loopEnd = NUM_STEPS;
    proc.getAnimalLoopBounds (loopStart, loopEnd);
    juce::ignoreUnused (loopStart, loopEnd);

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

    auto& pat = const_cast<DrumPattern&>(proc.drumEngine.getPattern());
    pat[(size_t) step][(size_t) drum].active   = ! pat[(size_t) step][(size_t) drum].active;
    pat[(size_t) step][(size_t) drum].velocity = 100;

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

// ─── LabelledKnob ─────────────────────────────────────────────────────────────

LabelledKnob::LabelledKnob (const juce::String& paramId, const juce::String& name,
                              juce::AudioProcessorValueTreeState& apvts)
{
    slider.setSliderStyle    (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, 52, 14);
    slider.setColour         (juce::Slider::textBoxTextColourId,    AnimalColors::TextDim);
    slider.setColour         (juce::Slider::textBoxBackgroundColourId, AnimalColors::Background);
    slider.setColour         (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText       (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour     (juce::Label::textColourId, AnimalColors::TextDim);
    label.setFont       (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible   (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                     (apvts, paramId, slider);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (14));
    slider.setBounds (b);
}

// ─── AnimalPanel ─────────────────────────────────────────────────────

AnimalPanel::AnimalPanel (BridgeProcessor& p)
    : proc (p),
      knobLoopStart ("loopStart", "Start", p.apvtsAnimal),
      knobLoopEnd   ("loopEnd",   "End",   p.apvtsAnimal),
      drumGrid (p),
      knobDensity     ("density",     "Density",   p.apvtsAnimal),
      knobSwing       ("swing",       "Swing",     p.apvtsAnimal),
      knobHumanize    ("humanize",    "Humanize",  p.apvtsAnimal),
      knobVelocity    ("velocity",    "Velocity",  p.apvtsAnimal),
      knobFillRate    ("fillRate",    "Fill Rate", p.apvtsAnimal),
      knobComplexity  ("complexity",  "Complexity",p.apvtsAnimal),
      knobPocket      ("pocket",      "Pocket",    p.apvtsAnimal),
      knobGhost       ("ghostAmount", "Ghost",     p.apvtsAnimal)
{
    setLookAndFeel (&laf);

    auto setupSectionLabel = [] (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
        lab.setColour (juce::Label::textColourId, AnimalColors::TextDim);
        lab.setJustificationType (juce::Justification::centredLeft);
    };
    setupSectionLabel (grooveLabel,     "GROOVE");
    setupSectionLabel (expressionLabel, "EXPRESSION");
    setupSectionLabel (loopingLabel,    "LOOPING");
    setupSectionLabel (actionsLabel,    "ACTIONS");
    addAndMakeVisible (grooveLabel);
    addAndMakeVisible (expressionLabel);
    addAndMakeVisible (loopingLabel);
    addAndMakeVisible (actionsLabel);

    addAndMakeVisible (knobLoopStart);
    addAndMakeVisible (knobLoopEnd);

    loopWidthLockButton.setClickingTogglesState (true);
    loopWidthLockButton.setTooltip ("Sync: when on, moving Start also moves End by the same amount (and vice versa).");
    loopWidthLockButton.setButtonText ("SYNC");
    loopWidthLockButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23202b));
    loopWidthLockButton.setColour (juce::TextButton::buttonOnColourId, AnimalColors::Accent.withAlpha (0.45f));
    loopWidthLockButton.setColour (juce::TextButton::textColourOffId, AnimalColors::TextDim);
    loopWidthLockButton.setColour (juce::TextButton::textColourOnId,  juce::Colours::white.withAlpha (0.95f));
    loopWidthLockAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                              (proc.apvtsAnimal, "loopWidthLock", loopWidthLockButton);
    addAndMakeVisible (loopWidthLockButton);

    proc.apvtsAnimal.addParameterListener ("loopStart", this);
    proc.apvtsAnimal.addParameterListener ("loopEnd", this);
    proc.apvtsAnimal.addParameterListener ("loopWidthLock", this);
    proc.apvtsAnimal.addParameterListener ("tickerSpeed", this);
    proc.apvtsAnimal.addParameterListener ("style", this);
    proc.apvtsAnimal.addParameterListener ("rootNote", this);
    proc.apvtsAnimal.addParameterListener ("scale", this);
    proc.apvtsAnimal.addParameterListener ("octave", this);

    proc.apvtsAnimal.state.addListener (this);

    addAndMakeVisible (drumGrid);

    rootNoteLabel.setText ("Root", juce::dontSendNotification);
    rootNoteLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    rootNoteLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (rootNoteLabel);

    static const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F",
                                        "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
        rootNoteBox.addItem (noteNames[i], i + 1);
    addAndMakeVisible (rootNoteBox);
    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                         (proc.apvtsAnimal, "rootNote", rootNoteBox);

    scaleLabel.setText ("Scale", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    scaleLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (scaleLabel);

    for (int i = 0; i < BootsyPreset::NUM_SCALES; ++i)
        scaleBox.addItem (BootsyPreset::SCALE_NAMES[i], i + 1);
    addAndMakeVisible (scaleBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsAnimal, "scale", scaleBox);

    octaveLabel.setText ("Octave", juce::dontSendNotification);
    octaveLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    octaveLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (octaveLabel);

    for (int o = 1; o <= 4; ++o)
        octaveBox.addItem (juce::String (o), o);
    addAndMakeVisible (octaveBox);
    octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                       (proc.apvtsAnimal, "octave", octaveBox);

    addAndMakeVisible (knobDensity);
    addAndMakeVisible (knobSwing);
    addAndMakeVisible (knobHumanize);
    addAndMakeVisible (knobVelocity);
    addAndMakeVisible (knobFillRate);
    addAndMakeVisible (knobComplexity);
    addAndMakeVisible (knobPocket);
    addAndMakeVisible (knobGhost);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsAnimal, "style", styleBox);

    generateButton.setTooltip ("Randomize settings and generate a new pattern.");
    generateButton.onClick = [this] { proc.triggerAnimalGenerate(); };

    performButton.setTooltip ("While transport runs: regenerate with seamless crossfade into the next loop.");
    performButton.setClickingTogglesState (true);
    performAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (proc.apvtsAnimal, "perform", performButton);

    fillButton.setTooltip ("Hold to layer live fills on top of the current groove.");
    fillButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    fillHoldListener = std::make_unique<FillHoldListener> (proc, fillButton);
    fillButton.addMouseListener (fillHoldListener.get(), false);

    addAndMakeVisible (generateButton);
    addAndMakeVisible (performButton);
    addAndMakeVisible (fillButton);

    syncLockedWidthFromParams();

    proc.rebuildAnimalGridPreview();
    stepTimer.startTimerHz (30);
}

AnimalPanel::~AnimalPanel()
{
    proc.apvtsAnimal.removeParameterListener ("loopStart", this);
    proc.apvtsAnimal.removeParameterListener ("loopEnd", this);
    proc.apvtsAnimal.removeParameterListener ("loopWidthLock", this);
    proc.apvtsAnimal.removeParameterListener ("tickerSpeed", this);
    proc.apvtsAnimal.removeParameterListener ("style", this);
    proc.apvtsAnimal.removeParameterListener ("rootNote", this);
    proc.apvtsAnimal.removeParameterListener ("scale", this);
    proc.apvtsAnimal.removeParameterListener ("octave", this);

    if (fillHoldListener != nullptr)
        fillButton.removeMouseListener (fillHoldListener.get());
    proc.apvtsAnimal.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void AnimalPanel::syncLockedWidthFromParams()
{
    int ls = (int) proc.apvtsAnimal.getRawParameterValue ("loopStart")->load();
    int le = (int) proc.apvtsAnimal.getRawParameterValue ("loopEnd")->load();
    ls = juce::jlimit (1, NUM_STEPS, ls);
    le = juce::jlimit (1, NUM_STEPS, le);
    if (le < ls) std::swap (ls, le);
    lockedLoopWidth = le - ls + 1;
}

void AnimalPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                                  const juce::String& id, int value)
{
    value = juce::jlimit (1, NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void AnimalPanel::applyLoopLockAfterStartChange()
{
    int ls = (int) proc.apvtsAnimal.getRawParameterValue ("loopStart")->load();
    ls = juce::jlimit (1, NUM_STEPS, ls);
    int le = ls + lockedLoopWidth - 1;
    if (le > NUM_STEPS)
    {
        le = NUM_STEPS;
        ls = juce::jmax (1, le - lockedLoopWidth + 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsAnimal, "loopStart", ls);
    setLoopIntParameter (proc.apvtsAnimal, "loopEnd", le);
    updatingLoopParams = false;
}

void AnimalPanel::applyLoopLockAfterEndChange()
{
    int le = (int) proc.apvtsAnimal.getRawParameterValue ("loopEnd")->load();
    le = juce::jlimit (1, NUM_STEPS, le);
    int ls = le - lockedLoopWidth + 1;
    if (ls < 1)
    {
        ls = 1;
        le = juce::jmin (NUM_STEPS, ls + lockedLoopWidth - 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsAnimal, "loopStart", ls);
    setLoopIntParameter (proc.apvtsAnimal, "loopEnd", le);
    updatingLoopParams = false;
}

void AnimalPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == "style")
    {
        const bool perf = proc.apvtsAnimal.getRawParameterValue ("perform")->load() > 0.5f;
        proc.rebuildAnimalGridPreview();
        proc.drumEngine.generatePattern (perf);
        proc.rebuildAnimalGridPreview();
        triggerAsyncUpdate();
        return;
    }

    if (parameterID == "rootNote" || parameterID == "octave" || parameterID == "scale")
    {
        drumGrid.repaint();
        triggerAsyncUpdate();
    }

    if (parameterID == "tickerSpeed")
        return;

    if (updatingLoopParams)
        return;

    if (parameterID == "loopWidthLock")
    {
        if ((bool) proc.apvtsAnimal.getRawParameterValue ("loopWidthLock")->load())
            syncLockedWidthFromParams();
        proc.rebuildAnimalGridPreview();
        triggerAsyncUpdate();
        return;
    }

    const bool lock = (bool) proc.apvtsAnimal.getRawParameterValue ("loopWidthLock")->load();
    if (lock)
    {
        if (parameterID == "loopStart")
            applyLoopLockAfterStartChange();
        else if (parameterID == "loopEnd")
            applyLoopLockAfterEndChange();
    }

    proc.rebuildAnimalGridPreview();
    triggerAsyncUpdate();
}

void AnimalPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildAnimalGridPreview();
    triggerAsyncUpdate();
}

namespace
{
    void layoutDropdownRow (juce::Rectangle<int> row,
                            juce::Label& rootLab, juce::ComboBox& rootBox,
                            juce::Label& scaleLab, juce::ComboBox& scaleBox,
                            juce::Label& octLab, juce::ComboBox& octBox,
                            juce::Label& styleLab, juce::ComboBox& styleBox)
    {
        const int h = bridge::instrumentLayout::kDropdownH;
        const int labY = row.getCentreY() - (int) (h * 0.5f);

        auto placeLabel = [&] (juce::Label& lab, int w)
        {
            lab.setBounds (row.removeFromLeft (w).withHeight (h).withY (labY));
            row.removeFromLeft (4);
        };
        auto placeBox = [&] (juce::ComboBox& box, int w)
        {
            box.setBounds (row.removeFromLeft (w).withHeight (h).withY (labY));
            row.removeFromLeft (14);
        };

        placeLabel (styleLab, 42);
        placeBox   (styleBox, 164);

        // Push Root/Scale/Oct to the right side of the row
        const int rightW = 360;
        if (row.getWidth() > rightW)
            row.removeFromLeft (row.getWidth() - rightW);

        placeLabel (rootLab, 42);
        placeBox   (rootBox, 60);
        placeLabel (scaleLab, 46);
        placeBox   (scaleBox, 120);
        placeLabel (octLab, 34);
        placeBox   (octBox, 56);
    }
}

void AnimalPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto outer = getLocalBounds().reduced (kPanelEdge, kPanelEdge);

    // 1. Dropdown row
    auto dropdownArea = outer.removeFromTop (kDropdownRowH);
    layoutDropdownRow (dropdownArea,
                       rootNoteLabel, rootNoteBox,
                       scaleLabel, scaleBox,
                       octaveLabel, octaveBox,
                       styleLabel, styleBox);
    outer.removeFromTop (kSectionGap);

    // 2. Main area — drum grid
    auto mainArea = outer.removeFromTop (kMainAreaH);
    drumGrid.setBounds (mainArea);
    outer.removeFromTop (kSectionGap);

    // 3. Bottom: knobs card (left) + loop/actions card (right)
    auto bottomArea = outer.removeFromTop (kBottomCardH);
    auto loopCard   = bottomArea.removeFromRight (kLoopCardW);
    bottomArea.removeFromRight (kCardGap);
    auto knobsCard  = bottomArea;

    // ── Knobs card: GROOVE row + EXPRESSION row ───────────────────────────
    {
        auto inner = knobsCard.reduced (14, 12);

        // GROOVE header + row of 5 knobs (Density/Swing/Humanize/Pocket/Velocity)
        grooveLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);
        const int halfH = (inner.getHeight() - kSectionHeaderH - 4 - 4) / 2;

        auto groove = inner.removeFromTop (halfH);
        {
            const int n = 5;
            const int gap = 6;
            const int kw = (groove.getWidth() - gap * (n - 1)) / n;
            knobDensity.setBounds  (groove.removeFromLeft (kw)); groove.removeFromLeft (gap);
            knobSwing.setBounds    (groove.removeFromLeft (kw)); groove.removeFromLeft (gap);
            knobHumanize.setBounds (groove.removeFromLeft (kw)); groove.removeFromLeft (gap);
            knobPocket.setBounds   (groove.removeFromLeft (kw)); groove.removeFromLeft (gap);
            knobVelocity.setBounds (groove.removeFromLeft (kw));
        }

        inner.removeFromTop (4);

        // EXPRESSION header + row of 3 knobs (Animal has no Staccato)
        expressionLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        auto expression = inner.removeFromTop (halfH);
        {
            const int n = 5;
            const int gap = 6;
            const int kw = (expression.getWidth() - gap * (n - 1)) / n;
            knobFillRate.setBounds   (expression.removeFromLeft (kw)); expression.removeFromLeft (gap);
            knobComplexity.setBounds (expression.removeFromLeft (kw)); expression.removeFromLeft (gap);
            knobGhost.setBounds      (expression.removeFromLeft (kw));
            // remaining space left blank — Animal has no Staccato knob
        }
    }

    // ── Loop/Actions card: LOOPING row + ACTIONS row ──────────────────────
    {
        auto inner = loopCard.reduced (14, 12);

        loopingLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        const int loopH = 96;
        auto loopRow = inner.removeFromTop (loopH);
        {
            const int knobW = (loopRow.getWidth() - kSyncBtnSide - 16) / 2;
            knobLoopStart.setBounds (loopRow.removeFromLeft (knobW));
            loopRow.removeFromLeft (8);
            auto syncCell = loopRow.removeFromLeft (kSyncBtnSide);
            loopWidthLockButton.setBounds (syncCell.withSizeKeepingCentre (kSyncBtnSide, kSyncBtnSide));
            loopRow.removeFromLeft (8);
            knobLoopEnd.setBounds (loopRow.removeFromLeft (knobW));
        }

        inner.removeFromTop (10);

        actionsLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        auto actionsRow = inner.removeFromTop (kBigActionBtnH);
        {
            const int n = 3;
            const int gap = 8;
            const int bw = (actionsRow.getWidth() - gap * (n - 1)) / n;
            generateButton.setBounds (actionsRow.removeFromLeft (bw)); actionsRow.removeFromLeft (gap);
            fillButton.setBounds     (actionsRow.removeFromLeft (bw)); actionsRow.removeFromLeft (gap);
            performButton.setBounds  (actionsRow.removeFromLeft (bw));
        }
    }
}

void AnimalPanel::paint (juce::Graphics& g)
{
    using namespace AnimalM3;
    using namespace bridge::instrumentLayout;
    g.fillAll (surface);

    // Paint backgrounds for the two bottom cards so they match the design system.
    auto full = getLocalBounds().reduced (kPanelEdge, kPanelEdge);
    full.removeFromTop (kDropdownRowH + kSectionGap + kMainAreaH + kSectionGap);
    auto bottom = full.removeFromTop (kBottomCardH);
    auto loopCard  = bottom.removeFromRight (kLoopCardW);
    bottom.removeFromRight (kCardGap);
    auto knobsCard = bottom;

    auto drawCard = [&] (juce::Rectangle<int> r)
    {
        auto rf = r.toFloat();
        drawShadow (g, rf, (float) kCardRadius, 1);
        fillSurface (g, rf, surfaceContainer, (float) kCardRadius);
        g.setColour (outline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), (float) kCardRadius, 1.0f);
    };
    drawCard (knobsCard);
    drawCard (loopCard);
}

void AnimalPanel::handleAsyncUpdate()
{
    drumGrid.repaint();
}

void AnimalPanel::updateStepAnimation()
{
    const int step = proc.animalCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        drumGrid.update (step);
    }

    const bool perfOn = proc.apvtsAnimal.getRawParameterValue ("perform")->load() > 0.5f;
    ++performBlinkTick;
    if (perfOn)
    {
        const bool flash = (performBlinkTick % (bridge::instrumentLayout::kPerformBlinkTicks * 2))
                            < bridge::instrumentLayout::kPerformBlinkTicks;
        const juce::Colour g (0xff4caf50);
        performButton.setColour (juce::TextButton::buttonColourId,
                                 flash ? g.withAlpha (0.82f) : g.withAlpha (0.30f));
        performButton.setColour (juce::TextButton::textColourOffId,
                                 juce::Colours::white.withAlpha (0.95f));
    }
    else
    {
        using namespace AnimalM3;
        performButton.setColour (juce::TextButton::buttonColourId,
                                 performButton.getToggleState() ? primary.withAlpha (0.35f)
                                                                : juce::Colours::transparentBlack);
        performButton.setColour (juce::TextButton::textColourOffId, AnimalColors::TextDim);
    }
}
