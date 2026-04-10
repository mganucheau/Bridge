#include "BootsyPanel.h"

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.bassEngine.setFillHoldActive (true);
            proc.rebuildBootsyGridPreview();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.bassEngine.setFillHoldActive (false);
            proc.rebuildBootsyGridPreview();
        }
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── PianoRollComponent ───────────────────────────────────────────────────────

PianoRollComponent::PianoRollComponent (BridgeProcessor& p) : proc (p) {}

bool PianoRollComponent::isBlackKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void PianoRollComponent::paint (juce::Graphics& g)
{
    using namespace BootsyM3;

    auto full = getLocalBounds().toFloat();
    g.setColour (surfaceContainerHigh);
    g.fillRoundedRectangle (full, cornerSmall);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRoundedRectangle (full.reduced (0.5f), cornerSmall, 1.0f);

    auto& engine = proc.bassEngine;
    int   root   = engine.degreeToMidiNote (0);
    int   low    = root - 18;
    int   high   = root + 24;
    low  = jlimit (0, 127, low);
    high = jlimit (0, 127, high);
    if (high <= low) return;

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int nRows = high - low + 1;
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

    g.setColour (primary.withAlpha (0.35f));
    const float ry = full.getY() + (float)(high - root) * rowH;
    g.fillRect (full.getX(), ry, full.getWidth(), rowH);
}

// ─── BassGridComponent ────────────────────────────────────────────────────────

BassGridComponent::BassGridComponent (BridgeProcessor& p)
    : proc (p)
{}

void BassGridComponent::paint (juce::Graphics& g)
{
    using namespace BootsyM3;

    auto& engine  = proc.bassEngine;
    auto& pattern = engine.getPatternForGrid();

    int loopStart = 1, loopEnd = BootsyPreset::NUM_STEPS;
    proc.getBootsyLoopBounds (loopStart, loopEnd);
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

    constexpr int DISPLAY_ROWS = 6;
    float cellW   = inner.getWidth() / (float) BootsyPreset::NUM_STEPS;
    float cellH   = hBody / (float) DISPLAY_ROWS;
    float pad     = 2.0f;

    static const int rowDegrees[DISPLAY_ROWS] = { 5, 4, 3, 2, 0, 7 };

    g.setColour (onSurfaceVariant);
    g.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
    {
        float cx = inner.getX() + step * cellW;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, inner.getY(), cellW, headerH - 2.0f),
                    juce::Justification::centred, false);
    }

    for (int row = 0; row < DISPLAY_ROWS; ++row)
    {
        float cy = gridTop + row * cellH;

        g.setColour (onSurfaceVariant.withAlpha (0.5f));
        g.setFont   (juce::Font (juce::FontOptions().withHeight (9.0f)));

        for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
        {
            float cx = inner.getX() + step * cellW + pad;
            auto  cell = juce::Rectangle<float> (cx, cy + pad,
                                                  cellW - pad * 2.0f, cellH - pad * 2.0f);

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

    for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
    {
        const BassHit& hit = pattern[(size_t) step];
        if (! hit.active) continue;

        int deg = hit.degree;
        int displayRow = 4;
        for (int r = 0; r < DISPLAY_ROWS; ++r)
            if (rowDegrees[r] == deg) { displayRow = r; break; }

        float cx = inner.getX() + step * cellW + pad;
        float cy = gridTop + displayRow * cellH + pad;
        auto  cell = juce::Rectangle<float> (cx, cy,
                                              cellW - pad * 2.0f, cellH - pad * 2.0f);

        auto col = BootsyColors::DegreeColors[jlimit (0, 7, deg)];

        float velFrac = hit.velocity / 127.0f;
        if (hit.isGhost)
            col = col.withAlpha (0.28f + velFrac * 0.25f);

        if (step == currentStep)
            col = col.brighter (0.25f);

        g.setColour (col);
        g.fillRoundedRectangle (cell, cornerExtraSmall);

        float velLineW = (cell.getWidth() - 2.0f) * velFrac;
        g.setColour (onSurface.withAlpha (0.30f));
        g.fillRect  (cell.getX() + 1.0f, cell.getY() + 1.0f, velLineW, 2.0f);

        g.setColour (hit.isGhost ? onSurfaceVariant.withAlpha (0.5f) : onSurface.withAlpha (0.9f));
        g.setFont   (juce::Font (juce::FontOptions().withHeight (9.0f)));
        g.drawFittedText (BootsyPreset::DEGREE_NAMES[jlimit (0, 7, deg)], cell.toNearestInt(),
                         juce::Justification::centred, 1);
    }

    if (currentStep >= 0 && currentStep < BootsyPreset::NUM_STEPS)
    {
        float cx = inner.getX() + currentStep * cellW;
        g.setColour (primary.withAlpha (0.08f));
        g.fillRect  (cx, gridTop, cellW, hBody);

        g.setColour (primary.withAlpha (0.85f));
        g.fillRect  (cx + pad, inner.getY(), cellW - pad * 2.0f, 2.0f);
    }

    const float legendX = full.getRight() - margin - (float)(BootsyPreset::NUM_DEGREES) * 26.0f;
    const float legendY = full.getBottom() - 14.0f;
    for (int d = 0; d < BootsyPreset::NUM_DEGREES; ++d)
    {
        float lx = legendX + d * 26.0f;
        auto legendCell = juce::Rectangle<float> (lx, legendY, 22.0f, 10.0f);
        g.setColour (BootsyColors::DegreeColors[d].withAlpha (0.75f));
        g.fillRoundedRectangle (legendCell, 3.0f);
        g.setColour (onSurface.withAlpha (0.7f));
        g.setFont   (juce::Font (juce::FontOptions().withHeight (8.0f)));
        g.drawFittedText (BootsyPreset::DEGREE_NAMES[d], legendCell.toNearestInt(),
                         juce::Justification::centred, 1);
    }
}

void BassGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.bassEngine;

    constexpr float margin  = 10.0f;
    constexpr float headerH = 26.0f;
    const float innerLeft  = margin;
    const float innerRight = (float)getWidth() - margin;
    const float gridTop    = margin + headerH;

    constexpr int DISPLAY_ROWS = 6;
    float cellW = (innerRight - innerLeft) / (float)BootsyPreset::NUM_STEPS;
    float gridH = (float)getHeight() - margin * 2.0f - headerH;
    float cellH = gridH / (float)DISPLAY_ROWS;

    int step = (int)((e.x - innerLeft) / cellW);
    int row  = (int)((e.y - gridTop) / cellH);

    if (e.y < gridTop) return;
    if (step < 0 || step >= BootsyPreset::NUM_STEPS || row < 0 || row >= DISPLAY_ROWS) return;

    auto& pat = const_cast<BassPattern&>(engine.getPattern());

    static const int rowDegrees[DISPLAY_ROWS] = { 5, 4, 3, 2, 0, 7 };
    int targetDeg = rowDegrees[row];

    if (e.mods.isRightButtonDown())
    {
        pat[(size_t)step].active = !pat[(size_t)step].active;
        if (pat[(size_t)step].active && pat[(size_t)step].velocity == 0)
            pat[(size_t)step].velocity = 100;
    }
    else
    {
        if (pat[(size_t)step].active && pat[(size_t)step].degree == targetDeg)
        {
            pat[(size_t)step].active = false;
        }
        else
        {
            pat[(size_t)step].active   = true;
            pat[(size_t)step].degree   = targetDeg;
            pat[(size_t)step].midiNote = engine.degreeToMidiNote (targetDeg);
            pat[(size_t)step].velocity = 100;
            pat[(size_t)step].isGhost  = false;
        }
    }

    proc.rebuildBootsyGridPreview();
    repaint();
}

void BassGridComponent::resized() {}

void BassGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── LabelledKnob ─────────────────────────────────────────────────────────────

BootsyLabelledKnob::BootsyLabelledKnob (const juce::String& paramId, const juce::String& name,
                              juce::AudioProcessorValueTreeState& apvts)
{
    slider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
    slider.setColour (juce::Slider::textBoxTextColourId,       BootsyColors::TextDim);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, BootsyColors::Background);
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText              (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour            (juce::Label::textColourId, BootsyColors::TextDim);
    label.setFont              (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                     (apvts, paramId, slider);
}

void BootsyLabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds  (b.removeFromBottom (14));
    slider.setBounds (b);
}

// ─── BootsyPanel ────────────────────────────────────────────────────────────

BootsyPanel::BootsyPanel (BridgeProcessor& p)
    : proc (p),
      pianoRoll (p),
      bassGrid (p),
      knobDensity     ("density",     "Density",   p.apvtsBootsy),
      knobSwing       ("swing",       "Swing",     p.apvtsBootsy),
      knobHumanize    ("humanize",    "Humanize",  p.apvtsBootsy),
      knobPocket      ("pocket",      "Pocket",    p.apvtsBootsy),
      knobVelocity    ("velocity",    "Velocity",  p.apvtsBootsy),
      knobComplexity  ("complexity",  "Complexity",p.apvtsBootsy),
      knobGhost       ("ghostAmount", "Ghost",     p.apvtsBootsy),
      knobStaccato    ("staccato",    "Staccato",  p.apvtsBootsy),
      knobFillRate    ("fillRate",    "Fill Rate", p.apvtsBootsy),
      knobLoopStart ("loopStart", "Start", p.apvtsBootsy),
      knobLoopEnd   ("loopEnd",   "End",   p.apvtsBootsy)
{
    setLookAndFeel (&laf);

    loopSectionLabel.setText ("Loop", juce::dontSendNotification);
    loopSectionLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    loopSectionLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
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
                              (proc.apvtsBootsy, "loopWidthLock", loopWidthLockButton);
    addAndMakeVisible (loopWidthLockButton);

    proc.apvtsBootsy.addParameterListener ("loopStart", this);
    proc.apvtsBootsy.addParameterListener ("loopEnd", this);
    proc.apvtsBootsy.addParameterListener ("loopWidthLock", this);
    proc.apvtsBootsy.addParameterListener ("tickerSpeed", this);
    proc.apvtsBootsy.addParameterListener ("rootNote", this);
    proc.apvtsBootsy.addParameterListener ("octave", this);
    proc.apvtsBootsy.addParameterListener ("scale", this);

    proc.apvtsBootsy.state.addListener (this);

    for (int i = 0; i < BootsyPreset::NUM_STYLES; ++i)
    {
        auto* btn = styleButtons.add (new juce::TextButton (BootsyPreset::STYLE_NAMES[i]));
        btn->setRadioGroupId (1);
        btn->setClickingTogglesState (true);
        btn->onClick = [this, i]
        {
            updateStyleButtonStates (i);
            proc.applyBootsyStyleAndRegenerate (i);
        };
        addAndMakeVisible (btn);
    }

    addAndMakeVisible (pianoRoll);
    addAndMakeVisible (bassGrid);

    rootNoteLabel.setText ("Root", juce::dontSendNotification);
    rootNoteLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    rootNoteLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (rootNoteLabel);

    static const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F",
                                        "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
        rootNoteBox.addItem (noteNames[i], i + 1);
    addAndMakeVisible (rootNoteBox);
    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                         (proc.apvtsBootsy, "rootNote", rootNoteBox);

    scaleLabel.setText ("Scale", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    scaleLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (scaleLabel);

    for (int i = 0; i < BootsyPreset::NUM_SCALES; ++i)
        scaleBox.addItem (BootsyPreset::SCALE_NAMES[i], i + 1);
    addAndMakeVisible (scaleBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsBootsy, "scale", scaleBox);

    octaveLabel.setText ("Octave", juce::dontSendNotification);
    octaveLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    octaveLabel.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (octaveLabel);

    for (int o = 1; o <= 4; ++o)
        octaveBox.addItem (juce::String (o), o);
    addAndMakeVisible (octaveBox);
    octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                       (proc.apvtsBootsy, "octave", octaveBox);

    addAndMakeVisible (knobDensity);
    addAndMakeVisible (knobSwing);
    addAndMakeVisible (knobHumanize);
    addAndMakeVisible (knobPocket);
    addAndMakeVisible (knobVelocity);
    addAndMakeVisible (knobComplexity);
    addAndMakeVisible (knobGhost);
    addAndMakeVisible (knobStaccato);
    addAndMakeVisible (knobFillRate);

    tickerLabel.setText ("Tick", juce::dontSendNotification);
    tickerLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    tickerLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    tickerLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tickerLabel);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    generateButton.setTooltip ("Randomize groove parameters and generate a new pattern.");
    generateButton.onClick = [this] { proc.triggerBootsyGenerate(); };

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

    knobLoopStart.slider.setRange (1, BootsyPreset::NUM_STEPS, 1);
    knobLoopEnd.slider.setRange (1, BootsyPreset::NUM_STEPS, 1);

    updateTickerButtonStates();
    syncLockedWidthFromParams();

    int curStyle = (int)*proc.apvtsBootsy.getRawParameterValue ("style");
    updateStyleButtonStates (curStyle);

    stepTimer.startTimerHz (30);
}

BootsyPanel::~BootsyPanel()
{
    proc.apvtsBootsy.removeParameterListener ("loopStart", this);
    proc.apvtsBootsy.removeParameterListener ("loopEnd", this);
    proc.apvtsBootsy.removeParameterListener ("loopWidthLock", this);
    proc.apvtsBootsy.removeParameterListener ("tickerSpeed", this);
    proc.apvtsBootsy.removeParameterListener ("rootNote", this);
    proc.apvtsBootsy.removeParameterListener ("octave", this);
    proc.apvtsBootsy.removeParameterListener ("scale", this);

    if (fillHoldListener != nullptr)
        fillButton.removeMouseListener (fillHoldListener.get());

    proc.apvtsBootsy.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void BootsyPanel::syncLockedWidthFromParams()
{
    int ls = (int) proc.apvtsBootsy.getRawParameterValue ("loopStart")->load();
    int le = (int) proc.apvtsBootsy.getRawParameterValue ("loopEnd")->load();
    ls = jlimit (1, BootsyPreset::NUM_STEPS, ls);
    le = jlimit (1, BootsyPreset::NUM_STEPS, le);
    if (le < ls)
        std::swap (ls, le);
    lockedLoopWidth = le - ls + 1;
}

void BootsyPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, BootsyPreset::NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void BootsyPanel::applyLoopLockAfterStartChange()
{
    int ls = (int) proc.apvtsBootsy.getRawParameterValue ("loopStart")->load();
    ls = jlimit (1, BootsyPreset::NUM_STEPS, ls);
    int le = ls + lockedLoopWidth - 1;
    if (le > BootsyPreset::NUM_STEPS)
    {
        le = BootsyPreset::NUM_STEPS;
        ls = jmax (1, le - lockedLoopWidth + 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsBootsy, "loopStart", ls);
    setLoopIntParameter (proc.apvtsBootsy, "loopEnd", le);
    updatingLoopParams = false;
}

void BootsyPanel::applyLoopLockAfterEndChange()
{
    int le = (int) proc.apvtsBootsy.getRawParameterValue ("loopEnd")->load();
    le = jlimit (1, BootsyPreset::NUM_STEPS, le);
    int ls = le - lockedLoopWidth + 1;
    if (ls < 1)
    {
        ls = 1;
        le = jmin (BootsyPreset::NUM_STEPS, ls + lockedLoopWidth - 1);
    }

    updatingLoopParams = true;
    setLoopIntParameter (proc.apvtsBootsy, "loopStart", ls);
    setLoopIntParameter (proc.apvtsBootsy, "loopEnd", le);
    updatingLoopParams = false;
}

void BootsyPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

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
        if ((bool) proc.apvtsBootsy.getRawParameterValue ("loopWidthLock")->load())
            syncLockedWidthFromParams();
        proc.rebuildBootsyGridPreview();
        triggerAsyncUpdate();
        return;
    }

    const bool lock = (bool) proc.apvtsBootsy.getRawParameterValue ("loopWidthLock")->load();
    if (lock)
    {
        if (parameterID == "loopStart")
            applyLoopLockAfterStartChange();
        else if (parameterID == "loopEnd")
            applyLoopLockAfterEndChange();
    }

    proc.rebuildBootsyGridPreview();
    triggerAsyncUpdate();
}

void BootsyPanel::setTickerSpeedChoice (int index)
{
    if (auto* p = proc.apvtsBootsy.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) index));
    updateTickerButtonStates();
}

void BootsyPanel::updateTickerButtonStates()
{
    int idx = 1;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsBootsy.getParameter ("tickerSpeed")))
        idx = c->getIndex();

    auto setOn = [] (juce::TextButton& b, bool on)
    {
        using namespace BootsyM3;
        b.setColour (juce::TextButton::buttonColourId,
                     on ? primary.withAlpha (0.35f) : juce::Colours::transparentBlack);
    };

    setOn (tickerFastButton,   idx == 0);
    setOn (tickerNormalButton, idx == 1);
    setOn (tickerSlowButton,   idx == 2);
}

void BootsyPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildBootsyGridPreview();
    triggerAsyncUpdate();
}

void BootsyPanel::resized()
{
    auto area = getLocalBounds().reduced (16);

    area.removeFromTop (8);

    const int bottomPanelH = 360;
    auto bottom = area.removeFromBottom (bottomPanelH);

    auto pitchRow = area.removeFromTop (34);
    rootNoteLabel.setBounds (pitchRow.removeFromLeft (32));
    rootNoteBox  .setBounds (pitchRow.removeFromLeft (65).reduced (0, 4));
    pitchRow.removeFromLeft (12);
    scaleLabel   .setBounds (pitchRow.removeFromLeft (36));
    scaleBox     .setBounds (pitchRow.removeFromLeft (100).reduced (0, 4));
    pitchRow.removeFromLeft (12);
    octaveLabel  .setBounds (pitchRow.removeFromLeft (44));
    octaveBox    .setBounds (pitchRow.removeFromLeft (55).reduced (0, 4));

    area.removeFromTop (8);

    auto gridRow = area;
    constexpr int pianoW = 56;
    pianoRoll.setBounds (gridRow.removeFromLeft (pianoW));
    gridRow.removeFromLeft (6);
    bassGrid.setBounds (gridRow);

    bottom.removeFromTop (6);

    auto knobRow1 = bottom.removeFromTop (98);
    int knobW = jmax (72, knobRow1.getWidth() / 5);
    knobDensity.setBounds    (knobRow1.removeFromLeft (knobW));
    knobSwing.setBounds      (knobRow1.removeFromLeft (knobW));
    knobHumanize.setBounds   (knobRow1.removeFromLeft (knobW));
    knobPocket.setBounds     (knobRow1.removeFromLeft (knobW));
    knobVelocity.setBounds   (knobRow1);

    bottom.removeFromTop (4);
    auto knobRow2 = bottom.removeFromTop (98);
    knobW = jmax (72, knobRow2.getWidth() / 4);
    knobFillRate.setBounds   (knobRow2.removeFromLeft (knobW));
    knobComplexity.setBounds (knobRow2.removeFromLeft (knobW));
    knobGhost.setBounds      (knobRow2.removeFromLeft (knobW));
    knobStaccato.setBounds   (knobRow2);

    bottom.removeFromTop (8);

    auto loopRow = bottom.removeFromTop (102);
    loopSectionLabel.setBounds (loopRow.removeFromLeft (40).withHeight (16));
    knobLoopStart.setBounds (loopRow.removeFromLeft (92));
    loopWidthLockButton.setBounds (loopRow.removeFromLeft (34).withSizeKeepingCentre (30, 30));
    knobLoopEnd.setBounds (loopRow.removeFromLeft (92));
    loopRow.removeFromLeft (10);

    tickerLabel.setBounds (loopRow.removeFromLeft (32).withHeight (18));
    auto tickStack = loopRow.removeFromLeft (42);
    const int tih = jmax (22, tickStack.getHeight() / 3);
    tickerFastButton.setBounds   (tickStack.removeFromTop (tih).reduced (0, 1));
    tickerNormalButton.setBounds (tickStack.removeFromTop (tih).reduced (0, 1));
    tickerSlowButton.setBounds   (tickStack.reduced (0, 1));

    loopRow.removeFromLeft (10);
    const int actW = jmin (88, loopRow.getWidth() / 2);
    generateButton.setBounds (loopRow.removeFromLeft (actW).reduced (0, 6));
    fillButton.setBounds     (loopRow.removeFromLeft (actW).reduced (0, 6));

    bottom.removeFromTop (6);

    auto styleRow = bottom.removeFromTop (36);
    styleLabel.setBounds (styleRow.removeFromLeft (48).reduced (0, 6));
    const int btnW = styleRow.getWidth() / BootsyPreset::NUM_STYLES;
    for (auto* btn : styleButtons)
        btn->setBounds (styleRow.removeFromLeft (btnW).reduced (2, 4));
}

void BootsyPanel::paint (juce::Graphics& g)
{
    using namespace BootsyM3;
    g.fillAll (surface);
}

void BootsyPanel::updateStyleButtonStates (int active)
{
    for (int i = 0; i < styleButtons.size(); ++i)
        styleButtons[i]->setToggleState (i == active, juce::dontSendNotification);
}

void BootsyPanel::handleAsyncUpdate()
{
    bassGrid.repaint();
    pianoRoll.repaint();
}

void BootsyPanel::updateStepAnimation()
{
    int step = proc.bootsyCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        bassGrid.update (step);
    }

    int curStyle = (int)*proc.apvtsBootsy.getRawParameterValue ("style");
    if (! styleButtons.isEmpty() && ! styleButtons[curStyle]->getToggleState())
        updateStyleButtonStates (curStyle);
}
