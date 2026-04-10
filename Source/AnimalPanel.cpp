#include "AnimalPanel.h"
#include "BridgeInstrumentStyles.h"
#include "MelodicGridLayout.h"
#include "bootsy/BootsyStylePresets.h"

namespace
{
bool drumMidiIsBlackKey (int midi)
{
    switch (midi % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void computeDrumGridGeometry (juce::Rectangle<float> fullBounds,
                              float& innerX, float& innerY, float& innerW, float& innerH,
                              float& gridTop, float& hBody,
                              float& originX, float& originY, float& cellSize,
                              float& pianoX, float& msX, float& msW,
                              float& leftBlockW)
{
    constexpr float margin  = 10.0f;
    constexpr float headerH = 26.0f;
    constexpr float pianoW  = 56.0f;
    constexpr float gap1    = 6.0f;
    constexpr float btnColW = 52.0f;
    constexpr float gap2    = 8.0f;

    auto inner = fullBounds.reduced (margin);
    innerX = inner.getX();
    innerY = inner.getY();
    innerW = inner.getWidth();
    innerH = inner.getHeight();
    gridTop = innerY + headerH;
    hBody = innerH - headerH;
    leftBlockW = pianoW + gap1 + btnColW + gap2;
    const float gridInnerX = innerX + leftBlockW;
    const float gridInnerW = innerW - leftBlockW;
    bridge::computeSquareMelodicGrid (gridInnerX, gridInnerW, gridTop, hBody,
                                      NUM_STEPS, NUM_DRUMS, originX, originY, cellSize);
    pianoX = innerX;
    msX = innerX + pianoW + gap1;
    msW = btnColW;
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
    return juce::jlimit (0, NUM_DRUMS - 1, NUM_DRUMS - 1 - visualRow);
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
    float pianoX = 0, msX = 0, msW = 0, leftBlockW = 0;
    constexpr float headerH = 26.0f;
    constexpr float pianoW  = 56.0f;
    constexpr float gap1    = 6.0f;
    constexpr float btnColW = 52.0f;
    const float keyNotesHeaderW = pianoW + gap1 + btnColW;

    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, pianoX, msX, msW, leftBlockW);

    const float pad = 2.0f;

    g.setColour (onSurfaceVariant);
    g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        float cx = originX + (float) step * cellSize;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, innerY, cellSize, headerH - 2.0f),
                    juce::Justification::centred, false);
    }

    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Semibold")));
    g.drawText ("Key notes", juce::Rectangle<float> (innerX, innerY, keyNotesHeaderW, headerH - 2.0f),
                juce::Justification::centred, false);

    // Piano strip: one row per drum lane; row height matches grid cellSize
    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        const int midi = DRUM_MIDI_NOTES[drum];
        float cy = originY + (float) visualRow * cellSize;
        auto row = juce::Rectangle<float> (pianoX, cy, pianoW, cellSize);
        bool bk = drumMidiIsBlackKey (midi);
        g.setColour (bk ? surfaceDim.withAlpha (0.88f) : surfaceContainerHighest.withAlpha (0.55f));
        g.fillRect (row);
        g.setColour (outlineVariant.withAlpha (0.28f));
        g.drawHorizontalLine ((int) (cy + cellSize), row.getX() + 1.0f, row.getRight() - 1.0f);
        g.setColour (onSurfaceVariant.withAlpha (0.88f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f)));
        static const char* n12[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        juce::String label = n12[midi % 12] + juce::String (midi / 12 - 1);
        g.drawText (label, row.reduced (2.0f, 0.0f), juce::Justification::centredRight, true);
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
    float pianoX = 0, msX = 0, msW = 0, leftBlockW = 0;
    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, pianoX, msX, msW, leftBlockW);

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
    constexpr int buttonW = 22;
    constexpr int buttonGap = 3;

    auto full = getLocalBounds().toFloat();
    float innerX = 0, innerY = 0, innerW = 0, innerH = 0;
    float gridTop = 0, hBody = 0;
    float originX = 0, originY = 0, cellSize = 0;
    float pianoX = 0, msX = 0, msW = 0, leftBlockW = 0;
    computeDrumGridGeometry (full, innerX, innerY, innerW, innerH, gridTop, hBody,
                             originX, originY, cellSize, pianoX, msX, msW, leftBlockW);

    const int rowH = (int) juce::jmax (1.0f, cellSize);

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        int y = (int) (originY + (float) visualRow * cellSize);
        auto row = juce::Rectangle<int> ((int) msX, y, (int) msW, rowH);
        auto controls = row.reduced (2, juce::jmax (0, (rowH - buttonW - buttonGap - buttonW) / 2));
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

    loopSectionLabel.setText ("Loop", juce::dontSendNotification);
    loopSectionLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    loopSectionLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    addAndMakeVisible (loopSectionLabel);

    addAndMakeVisible (knobLoopStart);
    addAndMakeVisible (knobLoopEnd);

    loopWidthLockButton.setClickingTogglesState (true);
    loopWidthLockButton.setTooltip ("Lock loop width: moving Start or End keeps the same number of steps.");
    loopWidthLockButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    juce::String lockUtf8 (juce::CharPointer_UTF8 ("🔒"));
    loopWidthLockButton.setButtonText (lockUtf8);
    loopWidthLockButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
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

    tickerLabel.setText ("Speed", juce::dontSendNotification);
    tickerLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    tickerLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    tickerLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tickerLabel);

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

    tickerFastButton.setTooltip ("Playhead moves 2x (visual only).");
    tickerNormalButton.setTooltip ("Playhead matches transport.");
    tickerSlowButton.setTooltip ("Playhead moves at half speed (visual only).");
    tickerFastButton.setClickingTogglesState (false);
    tickerNormalButton.setClickingTogglesState (false);
    tickerSlowButton.setClickingTogglesState (false);
    tickerFastButton.onClick   = [this] { setTickerSpeedChoice (0); };
    tickerNormalButton.onClick = [this] { setTickerSpeedChoice (1); };
    tickerSlowButton.onClick   = [this] { setTickerSpeedChoice (2); };

    addAndMakeVisible (generateButton);
    addAndMakeVisible (performButton);
    addAndMakeVisible (fillButton);
    addAndMakeVisible (tickerFastButton);
    addAndMakeVisible (tickerNormalButton);
    addAndMakeVisible (tickerSlowButton);

    updateTickerButtonStates();
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
        updateTickerButtonStates();

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

void AnimalPanel::setTickerSpeedChoice (int index)
{
    if (auto* p = proc.apvtsAnimal.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) index));
    updateTickerButtonStates();
}

void AnimalPanel::updateTickerButtonStates()
{
    int idx = 1;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsAnimal.getParameter ("tickerSpeed")))
        idx = c->getIndex();

    auto setOn = [] (juce::TextButton& b, bool on)
    {
        using namespace AnimalM3;
        b.setColour (juce::TextButton::buttonColourId,
                     on ? primary.withAlpha (0.35f) : juce::Colours::transparentBlack);
    };

    setOn (tickerFastButton,   idx == 0);
    setOn (tickerNormalButton, idx == 1);
    setOn (tickerSlowButton,   idx == 2);
}

void AnimalPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildAnimalGridPreview();
    triggerAsyncUpdate();
}

void AnimalPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (8);

    const int bottomH = kKnobRowH + kGap + kDropdownRow + kGap + kLoopRowH;
    auto bottom = area.removeFromBottom (bottomH);
    drumGrid.setBounds (area);

    bottom.removeFromTop (6);

    {
        auto knobRow = bottom.removeFromTop (kKnobRowH);
        const int gap = 6;
        const int n = 8;
        const int totalGaps = gap * (n - 1);
        int kw = (knobRow.getWidth() - totalGaps) / n;
        knobDensity.setBounds    (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobSwing.setBounds      (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobHumanize.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobPocket.setBounds     (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobVelocity.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobFillRate.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobComplexity.setBounds (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobGhost.setBounds      (knobRow.removeFromLeft (knobRow.getWidth()));
    }

    bottom.removeFromTop (kGap);

    {
        auto row = bottom.removeFromTop (kDropdownRow);
        const int dh = kDropdownH;
        const int yPad = (kDropdownRow - dh) / 2;
        auto placeCombo = [&] (juce::Label& lab, juce::ComboBox& box, int labW, int boxW)
        {
            lab.setBounds (row.removeFromLeft (labW).withHeight (kDropdownRow).reduced (0, 10));
            box.setBounds (row.removeFromLeft (boxW).withHeight (dh).translated (0, yPad));
            row.removeFromLeft (10);
        };
        placeCombo (rootNoteLabel, rootNoteBox, 36, 56);
        placeCombo (scaleLabel, scaleBox, 40, juce::jmin (120, juce::jmax (80, row.getWidth() / 6)));
        placeCombo (octaveLabel, octaveBox, 48, 52);
        placeCombo (styleLabel, styleBox, 40, juce::jmin (200, juce::jmax (120, row.getWidth() / 5)));
    }

    bottom.removeFromTop (kGap);

    {
        auto loopRow = bottom.removeFromTop (kLoopRowH);
        loopSectionLabel.setBounds (loopRow.removeFromLeft (36).withHeight (14).translated (0, 4));

        knobLoopStart.setBounds (loopRow.removeFromLeft (86));
        loopRow.removeFromLeft (4);
        loopWidthLockButton.setBounds (loopRow.removeFromLeft (30).withSizeKeepingCentre (26, 26));
        loopRow.removeFromLeft (4);
        knobLoopEnd.setBounds (loopRow.removeFromLeft (86));
        loopRow.removeFromLeft (10);

        const int speedBlockW = kSpeedBlockW;
        auto speedBlock = loopRow.removeFromLeft (speedBlockW);
        tickerLabel.setBounds (speedBlock.removeFromTop (14));
        {
            auto strip = speedBlock;
            const int gapS = kSpeedBtnGap;
            const int bw = (strip.getWidth() - gapS * 2) / 3;
            const int bh = juce::jmin (bw, strip.getHeight());
            const int x0 = strip.getX();
            const int y0 = strip.getY() + (strip.getHeight() - bh) / 2;
            tickerFastButton.setBounds   (x0, y0, bw, bh);
            tickerNormalButton.setBounds (x0 + bw + gapS, y0, bw, bh);
            tickerSlowButton.setBounds   (x0 + 2 * (bw + gapS), y0, bw, bh);
        }

        loopRow.removeFromLeft (kActionBtnGap);

        const int sq = kActionBtnSide;
        const int gapA = kActionBtnGap;
        const int rowTop = loopRow.getY();
        const int yOff = (kLoopRowH - sq) / 2;
        generateButton.setBounds (loopRow.removeFromLeft (sq).withY (rowTop + yOff).withHeight (sq));
        loopRow.removeFromLeft (gapA);
        fillButton.setBounds     (loopRow.removeFromLeft (sq).withY (rowTop + yOff).withHeight (sq));
        loopRow.removeFromLeft (gapA);
        performButton.setBounds  (loopRow.removeFromLeft (sq).withY (rowTop + yOff).withHeight (sq));
    }
}

void AnimalPanel::paint (juce::Graphics& g)
{
    using namespace AnimalM3;
    g.fillAll (surface);
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
