#include "PaulPanel.h"
#include "BridgeInstrumentStyles.h"
#include "MelodicGridLayout.h"

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.guitarEngine.setFillHoldActive (true);
            proc.rebuildPaulGridPreview();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.guitarEngine.setFillHoldActive (false);
            proc.rebuildPaulGridPreview();
        }
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── PaulPianoRollComponent ───────────────────────────────────────────────────────

PaulPianoRollComponent::PaulPianoRollComponent (BridgeProcessor& p) : proc (p) {}

bool PaulPianoRollComponent::isBlackKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void PaulPianoRollComponent::paint (juce::Graphics& g)
{
    using namespace PaulM3;

    auto full = getLocalBounds().toFloat();
    g.setColour (surfaceContainerHigh);
    g.fillRoundedRectangle (full, cornerSmall);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRoundedRectangle (full.reduced (0.5f), cornerSmall, 1.0f);

    auto& engine = proc.guitarEngine;
    int low = 60, high = 72;
    bridge::setOneOctaveMelodicRange (engine, low, high);
    if (high <= low) return;

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int nRows = bridge::kMelodicOctaveRows;
    const float rowH = full.getHeight() / (float) nRows;
    const float leftPad = 2.0f;

    for (int m = high; m >= low; --m)
    {
        int idx = high - m;
        float y = full.getY() + (float) idx * rowH;
        auto row = juce::Rectangle<float> (full.getX(), y, full.getWidth(), rowH);

        bool bk = isBlackKey (m);
        g.setColour (bk ? surfaceDim.withAlpha (0.85f) : surfaceContainerHighest.withAlpha (0.5f));
        g.fillRect (row);

        g.setColour (outlineVariant.withAlpha (0.25f));
        g.drawHorizontalLine ((int)(y + rowH), full.getX() + leftPad, full.getRight() - 1.0f);

        g.setColour (onSurfaceVariant.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f)));
        g.drawText (names[m % 12] + juce::String (m / 12 - 1),
                    row.reduced (leftPad, 0.0f).removeFromRight (full.getWidth() * 0.45f),
                    juce::Justification::centredRight, true);
    }

    const int root = engine.degreeToMidiNote (0, -1);
    if (root >= low && root <= high)
    {
        g.setColour (primary.withAlpha (0.35f));
        const float ry = full.getY() + (float)(high - root) * rowH;
        g.fillRect (full.getX(), ry, full.getWidth(), rowH);
    }
}

// ─── PaulBassGridComponent ────────────────────────────────────────────────────────

PaulBassGridComponent::PaulBassGridComponent (BridgeProcessor& p)
    : proc (p)
{}

void PaulBassGridComponent::paint (juce::Graphics& g)
{
    using namespace PaulM3;

    auto& engine  = proc.guitarEngine;
    auto& pattern = engine.getPatternForGrid();

    int loopStart = 1, loopEnd = PaulPreset::NUM_STEPS;
    proc.getPaulLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    drawShadow  (g, full, cornerLarge, 1);
    fillSurface (g, full, surfaceContainer, cornerLarge);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRoundedRectangle (full.reduced (0.5f), cornerLarge, 1.0f);

    constexpr float margin  = 10.0f;
    constexpr float headerH = 26.0f;
    auto inner = full.reduced (margin);

    float gridTop = inner.getY() + headerH;
    float hBody   = inner.getHeight() - headerH;

    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;

    float originX = 0.0f, originY = 0.0f, cellSize = 1.0f;
    bridge::computeSquareMelodicGrid (inner.getX(), inner.getWidth(), gridTop, hBody,
                                      PaulPreset::NUM_STEPS, nRows, originX, originY, cellSize);
    float pad   = 2.0f;

    g.setColour (onSurfaceVariant);
    g.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    for (int step = 0; step < PaulPreset::NUM_STEPS; ++step)
    {
        float cx = originX + (float) step * cellSize;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, inner.getY(), cellSize, headerH - 2.0f),
                    juce::Justification::centred, false);
    }

    for (int row = 0; row < nRows; ++row)
    {
        float cy = originY + (float) row * cellSize;
        for (int step = 0; step < PaulPreset::NUM_STEPS; ++step)
        {
            float cx = originX + (float) step * cellSize + pad;
            auto  cell = juce::Rectangle<float> (cx, cy + pad,
                                                  cellSize - pad * 2.0f, cellSize - pad * 2.0f);

            bool isBeat  = (step % 4 == 0);
            bool inLoop  = (step >= ls0 && step <= le0);

            if (! inLoop)
            {
                auto baseDim = surfaceDim.interpolatedWith (surfaceContainerHigh, 0.45f);
                g.setColour (baseDim);
                g.fillRoundedRectangle (cell, cornerExtraSmall);
                g.setColour (outlineVariant.withAlpha (0.25f));
                g.drawRoundedRectangle (cell, cornerExtraSmall, 1.0f);
                continue;
            }

            auto baseCol = isBeat ? surfaceContainerHighest : surfaceContainerHigh;
            if (step == currentStep)
                baseCol = primary.withAlpha (0.10f);

            g.setColour (baseCol);
            g.fillRoundedRectangle (cell, cornerExtraSmall);

            g.setColour ((isBeat ? outline.withAlpha (0.50f) : outlineVariant.withAlpha (0.40f)));
            g.drawRoundedRectangle (cell, cornerExtraSmall, 1.0f);
        }
    }

    for (int step = 0; step < PaulPreset::NUM_STEPS; ++step)
    {
        const GuitarHit& hit = pattern[(size_t) step];
        if (! hit.active) continue;

        const int midi = juce::jlimit (minMidi, maxMidi, hit.midiNote);
        int displayRow = maxMidi - midi;
        displayRow = juce::jlimit (0, nRows - 1, displayRow);

        float cx = originX + (float) step * cellSize + pad;
        float cy = originY + (float) displayRow * cellSize + pad;
        auto  cell = juce::Rectangle<float> (cx, cy,
                                              cellSize - pad * 2.0f, cellSize - pad * 2.0f);

        juce::Colour col = primary.withAlpha (hit.isGhost ? 0.35f : 0.72f);
        float velFrac = hit.velocity / 127.0f;
        if (hit.isGhost)
            col = col.withAlpha (0.28f + velFrac * 0.25f);

        if (step == currentStep)
            col = col.brighter (0.2f);

        g.setColour (col);
        g.fillRoundedRectangle (cell, cornerExtraSmall);

        float velLineW = (cell.getWidth() - 2.0f) * velFrac;
        g.setColour (onSurface.withAlpha (0.30f));
        g.fillRect  (cell.getX() + 1.0f, cell.getY() + 1.0f, velLineW, 2.0f);
    }

    if (currentStep >= 0 && currentStep < PaulPreset::NUM_STEPS)
    {
        float cx = originX + (float) currentStep * cellSize;
        g.setColour (primary.withAlpha (0.08f));
        g.fillRect  (cx, originY, cellSize, cellSize * (float) nRows);

        g.setColour (primary.withAlpha (0.85f));
        g.fillRect  (cx + pad, inner.getY(), cellSize - pad * 2.0f, 2.0f);
    }
}

void PaulBassGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.guitarEngine;

    constexpr float margin  = 10.0f;
    constexpr float headerH = 26.0f;
    const float innerLeft  = margin;
    const float innerRight = (float)getWidth() - margin;
    const float innerW     = innerRight - innerLeft;
    const float gridTop    = margin + headerH;
    const float hBody      = (float)getHeight() - margin * 2.0f - headerH;

    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;

    float originX = 0.0f, originY = 0.0f, cellSize = 1.0f;
    bridge::computeSquareMelodicGrid (innerLeft, innerW, gridTop, hBody,
                                      PaulPreset::NUM_STEPS, nRows, originX, originY, cellSize);

    int step = (int)((e.x - originX) / cellSize);
    int row  = (int)((e.y - originY) / cellSize);

    if (e.y < gridTop || e.y > originY + cellSize * (float) nRows) return;
    if (step < 0 || step >= PaulPreset::NUM_STEPS || row < 0 || row >= nRows) return;

    auto& pat = const_cast<GuitarPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;

    if (e.mods.isRightButtonDown())
    {
        pat[(size_t)step].active = !pat[(size_t)step].active;
        if (pat[(size_t)step].active && pat[(size_t)step].velocity == 0)
            pat[(size_t)step].velocity = 100;
    }
    else
    {
        if (pat[(size_t)step].active && pat[(size_t)step].midiNote == targetMidi)
        {
            pat[(size_t)step].active = false;
        }
        else
        {
            const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
            pat[(size_t)step].active   = true;
            pat[(size_t)step].degree   = deg;
            pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
            pat[(size_t)step].velocity = 100;
            pat[(size_t)step].isGhost  = false;
        }
    }

    proc.rebuildPaulGridPreview();
    repaint();
}

void PaulBassGridComponent::resized() {}

void PaulBassGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── LabelledKnob ─────────────────────────────────────────────────────────────

PaulLabelledKnob::PaulLabelledKnob (const juce::String& paramId, const juce::String& name,
                              juce::AudioProcessorValueTreeState& apvts)
{
    slider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
    slider.setColour (juce::Slider::textBoxTextColourId,       PaulColors::TextDim);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, PaulColors::Background);
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText              (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour            (juce::Label::textColourId, PaulColors::TextDim);
    label.setFont              (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                     (apvts, paramId, slider);
}

void PaulLabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds  (b.removeFromBottom (14));
    slider.setBounds (b);
}

// ─── PaulPanel ────────────────────────────────────────────────────────────

PaulPanel::PaulPanel (BridgeProcessor& p)
    : proc (p),
      pianoRoll (p),
      bassGrid (p),
      knobDensity     ("density",     "Density",   p.apvtsPaul),
      knobSwing       ("swing",       "Swing",     p.apvtsPaul),
      knobHumanize    ("humanize",    "Humanize",  p.apvtsPaul),
      knobPocket      ("pocket",      "Pocket",    p.apvtsPaul),
      knobVelocity    ("velocity",    "Velocity",  p.apvtsPaul),
      knobComplexity  ("complexity",  "Complexity",p.apvtsPaul),
      knobGhost       ("ghostAmount", "Ghost",     p.apvtsPaul),
      knobStaccato    ("staccato",    "Staccato",  p.apvtsPaul),
      knobFillRate    ("fillRate",    "Fill Rate", p.apvtsPaul),
      knobLoopStart ("loopStart", "Start", p.apvtsPaul),
      knobLoopEnd   ("loopEnd",   "End",   p.apvtsPaul)
{
    setLookAndFeel (&laf);

    loopSectionLabel.setText ("Loop", juce::dontSendNotification);
    loopSectionLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    loopSectionLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    addAndMakeVisible (loopSectionLabel);

    addAndMakeVisible (knobLoopStart);
    addAndMakeVisible (knobLoopEnd);

    loopWidthLockButton.setClickingTogglesState (true);
    loopWidthLockButton.setTooltip ("Lock loop width: moving Start or End keeps the same number of steps.");
    loopWidthLockButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    {
        juce::String lockUtf8 (juce::CharPointer_UTF8 ("🔒"));
        loopWidthLockButton.setButtonText (lockUtf8);
    }
    loopWidthLockButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loopWidthLockAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                              (proc.apvtsPaul, "loopWidthLock", loopWidthLockButton);
    addAndMakeVisible (loopWidthLockButton);

    proc.apvtsPaul.addParameterListener ("loopStart", this);
    proc.apvtsPaul.addParameterListener ("loopEnd", this);
    proc.apvtsPaul.addParameterListener ("loopWidthLock", this);
    proc.apvtsPaul.addParameterListener ("tickerSpeed", this);
    proc.apvtsPaul.addParameterListener ("rootNote", this);
    proc.apvtsPaul.addParameterListener ("octave", this);
    proc.apvtsPaul.addParameterListener ("scale", this);
    proc.apvtsPaul.addParameterListener ("style", this);

    proc.apvtsPaul.state.addListener (this);

    addAndMakeVisible (pianoRoll);
    addAndMakeVisible (bassGrid);

    rootNoteLabel.setText ("Root", juce::dontSendNotification);
    rootNoteLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    rootNoteLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (rootNoteLabel);

    static const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F",
                                        "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
        rootNoteBox.addItem (noteNames[i], i + 1);
    addAndMakeVisible (rootNoteBox);
    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                         (proc.apvtsPaul, "rootNote", rootNoteBox);

    scaleLabel.setText ("Scale", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    scaleLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (scaleLabel);

    for (int i = 0; i < PaulPreset::NUM_SCALES; ++i)
        scaleBox.addItem (PaulPreset::SCALE_NAMES[i], i + 1);
    addAndMakeVisible (scaleBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsPaul, "scale", scaleBox);

    octaveLabel.setText ("Octave", juce::dontSendNotification);
    octaveLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    octaveLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (octaveLabel);

    for (int o = 1; o <= 4; ++o)
        octaveBox.addItem (juce::String (o), o);
    addAndMakeVisible (octaveBox);
    octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                       (proc.apvtsPaul, "octave", octaveBox);

    addAndMakeVisible (knobDensity);
    addAndMakeVisible (knobSwing);
    addAndMakeVisible (knobHumanize);
    addAndMakeVisible (knobPocket);
    addAndMakeVisible (knobVelocity);
    addAndMakeVisible (knobComplexity);
    addAndMakeVisible (knobGhost);
    addAndMakeVisible (knobStaccato);
    addAndMakeVisible (knobFillRate);

    tickerLabel.setText ("Speed", juce::dontSendNotification);
    tickerLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    tickerLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    tickerLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tickerLabel);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, PaulColors::TextDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsPaul, "style", styleBox);

    generateButton.setTooltip ("Randomize groove parameters and generate a new pattern.");
    generateButton.onClick = [this] { proc.triggerPaulGenerate(); };

    performButton.setTooltip ("While transport runs: regenerate with seamless crossfade into the next loop.");
    performButton.setClickingTogglesState (true);
    performAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (proc.apvtsPaul, "perform", performButton);
    addAndMakeVisible (performButton);

    fillButton.setTooltip ("Hold to layer live fills on top of the current line.");
    fillButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    fillHoldListener = std::make_unique<FillHoldListener> (proc, fillButton);
    fillButton.addMouseListener (fillHoldListener.get(), false);

    tickerFastButton.setTooltip ("Playhead moves 2x (visual only).");
    tickerNormalButton.setTooltip ("Playhead matches transport.");
    tickerSlowButton.setTooltip ("Playhead moves at half speed (visual only).");
    tickerFastButton.onClick   = [this] { setTickerSpeedChoice (0); };
    tickerNormalButton.onClick = [this] { setTickerSpeedChoice (1); };
    tickerSlowButton.onClick   = [this] { setTickerSpeedChoice (2); };

    addAndMakeVisible (generateButton);
    addAndMakeVisible (fillButton);
    addAndMakeVisible (tickerFastButton);
    addAndMakeVisible (tickerNormalButton);
    addAndMakeVisible (tickerSlowButton);

    knobLoopStart.slider.setRange (1, PaulPreset::NUM_STEPS, 1);
    knobLoopEnd.slider.setRange (1, PaulPreset::NUM_STEPS, 1);

    updateTickerButtonStates();
    syncLockedWidthFromParams();

    stepTimer.startTimerHz (30);
}

PaulPanel::~PaulPanel()
{
    proc.apvtsPaul.removeParameterListener ("loopStart", this);
    proc.apvtsPaul.removeParameterListener ("loopEnd", this);
    proc.apvtsPaul.removeParameterListener ("loopWidthLock", this);
    proc.apvtsPaul.removeParameterListener ("tickerSpeed", this);
    proc.apvtsPaul.removeParameterListener ("rootNote", this);
    proc.apvtsPaul.removeParameterListener ("octave", this);
    proc.apvtsPaul.removeParameterListener ("scale", this);
    proc.apvtsPaul.removeParameterListener ("style", this);

    if (fillHoldListener != nullptr)
        fillButton.removeMouseListener (fillHoldListener.get());

    proc.apvtsPaul.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void PaulPanel::syncLockedWidthFromParams()
{
    int ls = (int) proc.apvtsPaul.getRawParameterValue ("loopStart")->load();
    int le = (int) proc.apvtsPaul.getRawParameterValue ("loopEnd")->load();
    ls = jlimit (1, PaulPreset::NUM_STEPS, ls);
    le = jlimit (1, PaulPreset::NUM_STEPS, le);
    if (le < ls)
        std::swap (ls, le);
    lockedLoopWidth = le - ls + 1;
}

void PaulPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, PaulPreset::NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void PaulPanel::applyLoopLockAfterStartChange()
{
    int ls = (int) proc.apvtsPaul.getRawParameterValue ("loopStart")->load();
    ls = jlimit (1, PaulPreset::NUM_STEPS, ls);
    int le = ls + lockedLoopWidth - 1;
    if (le > PaulPreset::NUM_STEPS)
    {
        le = PaulPreset::NUM_STEPS;
        ls = jmax (1, le - lockedLoopWidth + 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsPaul, "loopStart", ls);
    setLoopIntParameter (proc.apvtsPaul, "loopEnd", le);
    updatingLoopParams = false;
}

void PaulPanel::applyLoopLockAfterEndChange()
{
    int le = (int) proc.apvtsPaul.getRawParameterValue ("loopEnd")->load();
    le = jlimit (1, PaulPreset::NUM_STEPS, le);
    int ls = le - lockedLoopWidth + 1;
    if (ls < 1)
    {
        ls = 1;
        le = jmin (PaulPreset::NUM_STEPS, ls + lockedLoopWidth - 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsPaul, "loopStart", ls);
    setLoopIntParameter (proc.apvtsPaul, "loopEnd", le);
    updatingLoopParams = false;
}

void PaulPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == "style")
    {
        proc.rebuildPaulGridPreview();
        proc.guitarEngine.generatePattern();
        proc.rebuildPaulGridPreview();
        triggerAsyncUpdate();
        return;
    }

    if (parameterID == "rootNote" || parameterID == "octave" || parameterID == "scale")
    {
        pianoRoll.repaint();
        triggerAsyncUpdate();
    }

    if (parameterID == "tickerSpeed")
        updateTickerButtonStates();

    if (updatingLoopParams)
        return;

    if (parameterID == "loopWidthLock")
    {
        if ((bool) proc.apvtsPaul.getRawParameterValue ("loopWidthLock")->load())
            syncLockedWidthFromParams();
        proc.rebuildPaulGridPreview();
        triggerAsyncUpdate();
        return;
    }

    const bool lock = (bool) proc.apvtsPaul.getRawParameterValue ("loopWidthLock")->load();
    if (lock)
    {
        if (parameterID == "loopStart")
            applyLoopLockAfterStartChange();
        else if (parameterID == "loopEnd")
            applyLoopLockAfterEndChange();
    }

    proc.rebuildPaulGridPreview();
    triggerAsyncUpdate();
}

void PaulPanel::setTickerSpeedChoice (int index)
{
    if (auto* p = proc.apvtsPaul.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) index));
    updateTickerButtonStates();
}

void PaulPanel::updateTickerButtonStates()
{
    int idx = 1;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsPaul.getParameter ("tickerSpeed")))
        idx = c->getIndex();

    auto setOn = [] (juce::TextButton& b, bool on)
    {
        using namespace PaulM3;
        b.setColour (juce::TextButton::buttonColourId,
                     on ? primary.withAlpha (0.35f) : juce::Colours::transparentBlack);
    };

    setOn (tickerFastButton,   idx == 0);
    setOn (tickerNormalButton, idx == 1);
    setOn (tickerSlowButton,   idx == 2);
}

void PaulPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildPaulGridPreview();
    triggerAsyncUpdate();
}

void PaulPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (8);

    const int bottomH = kKnobRowH + kGap + kDropdownRow + kGap + kLoopRowH;
    auto bottom = area.removeFromBottom (bottomH);

    auto gridRow = area;
    constexpr int pianoW = 56;
    pianoRoll.setBounds (gridRow.removeFromLeft (pianoW));
    gridRow.removeFromLeft (6);
    bassGrid.setBounds (gridRow);

    bottom.removeFromTop (6);

    {
        auto knobRow = bottom.removeFromTop (kKnobRowH);
        const int gap = 6;
        const int n = 9;
        const int totalGaps = gap * (n - 1);
        int kw = (knobRow.getWidth() - totalGaps) / n;
        knobDensity.setBounds    (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobSwing.setBounds      (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobHumanize.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobPocket.setBounds     (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobVelocity.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobFillRate.setBounds   (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobComplexity.setBounds (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobGhost.setBounds      (knobRow.removeFromLeft (kw)); knobRow.removeFromLeft (gap);
        knobStaccato.setBounds   (knobRow.removeFromLeft (knobRow.getWidth()));
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

void PaulPanel::paint (juce::Graphics& g)
{
    using namespace PaulM3;
    g.fillAll (surface);
}

void PaulPanel::handleAsyncUpdate()
{
    bassGrid.repaint();
    pianoRoll.repaint();
}

void PaulPanel::updateStepAnimation()
{
    int step = proc.paulCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        bassGrid.update (step);
    }

    const bool perfOn = proc.apvtsPaul.getRawParameterValue ("perform")->load() > 0.5f;
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
        using namespace PaulM3;
        performButton.setColour (juce::TextButton::buttonColourId,
                                 performButton.getToggleState() ? primary.withAlpha (0.35f)
                                                                : juce::Colours::transparentBlack);
        performButton.setColour (juce::TextButton::textColourOffId, PaulColors::TextDim);
    }
}
