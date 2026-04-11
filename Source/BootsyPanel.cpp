#include "BootsyPanel.h"
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

    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;

    float originX = 0.0f, originY = 0.0f, cellSize = 1.0f;
    bridge::computeSquareMelodicGrid (inner.getX(), inner.getWidth(), gridTop, hBody,
                                      BootsyPreset::NUM_STEPS, nRows, originX, originY, cellSize);
    float pad   = 2.0f;

    g.setColour (onSurfaceVariant);
    g.setFont   (juce::Font (juce::FontOptions().withHeight (11.0f)));
    for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
    {
        float cx = originX + (float) step * cellSize;
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (cx, inner.getY(), cellSize, headerH - 2.0f),
                    juce::Justification::centred, false);
    }

    for (int row = 0; row < nRows; ++row)
    {
        float cy = originY + (float) row * cellSize;
        for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
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

    for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
    {
        const BassHit& hit = pattern[(size_t) step];
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

    if (currentStep >= 0 && currentStep < BootsyPreset::NUM_STEPS)
    {
        float cx = originX + (float) currentStep * cellSize;
        g.setColour (primary.withAlpha (0.08f));
        g.fillRect  (cx, originY, cellSize, cellSize * (float) nRows);

        g.setColour (primary.withAlpha (0.85f));
        g.fillRect  (cx + pad, inner.getY(), cellSize - pad * 2.0f, 2.0f);
    }
}

void BassGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.bassEngine;

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
                                      BootsyPreset::NUM_STEPS, nRows, originX, originY, cellSize);

    int step = (int)((e.x - originX) / cellSize);
    int row  = (int)((e.y - originY) / cellSize);

    if (e.y < gridTop || e.y > originY + cellSize * (float) nRows) return;
    if (step < 0 || step >= BootsyPreset::NUM_STEPS || row < 0 || row >= nRows) return;

    auto& pat = const_cast<BassPattern&>(engine.getPattern());
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

    auto setupSectionLabel = [] (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
        lab.setColour (juce::Label::textColourId, BootsyColors::TextDim);
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
    loopWidthLockButton.setColour (juce::TextButton::buttonOnColourId, BootsyM3::primary.withAlpha (0.45f));
    loopWidthLockButton.setColour (juce::TextButton::textColourOffId, BootsyColors::TextDim);
    loopWidthLockButton.setColour (juce::TextButton::textColourOnId,  juce::Colours::white.withAlpha (0.95f));
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
    proc.apvtsBootsy.addParameterListener ("style", this);

    proc.apvtsBootsy.state.addListener (this);

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

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, BootsyColors::TextDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsBootsy, "style", styleBox);

    generateButton.setTooltip ("Randomize groove parameters and generate a new pattern.");
    generateButton.onClick = [this] { proc.triggerBootsyGenerate(); };

    performButton.setTooltip ("While transport runs: regenerate with seamless crossfade into the next loop.");
    performButton.setClickingTogglesState (true);
    performAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (proc.apvtsBootsy, "perform", performButton);
    addAndMakeVisible (performButton);

    fillButton.setTooltip ("Hold to layer live fills on top of the current line.");
    fillButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    fillHoldListener = std::make_unique<FillHoldListener> (proc, fillButton);
    fillButton.addMouseListener (fillHoldListener.get(), false);

    addAndMakeVisible (generateButton);
    addAndMakeVisible (fillButton);

    knobLoopStart.slider.setRange (1, BootsyPreset::NUM_STEPS, 1);
    knobLoopEnd.slider.setRange (1, BootsyPreset::NUM_STEPS, 1);

    syncLockedWidthFromParams();

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
    proc.apvtsBootsy.removeParameterListener ("style", this);

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

    if (parameterID == "style")
    {
        proc.rebuildBootsyGridPreview();
        proc.bassEngine.generatePattern();
        proc.rebuildBootsyGridPreview();
        triggerAsyncUpdate();
        return;
    }

    if (parameterID == "rootNote" || parameterID == "octave" || parameterID == "scale")
    {
        pianoRoll.repaint();
        triggerAsyncUpdate();
    }

    if (parameterID == "tickerSpeed")
        return;

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

void BootsyPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildBootsyGridPreview();
    triggerAsyncUpdate();
}

namespace
{
    void layoutBootsyDropdownRow (juce::Rectangle<int> row,
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

void BootsyPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto outer = getLocalBounds().reduced (kPanelEdge, kPanelEdge);

    // 1. Dropdown row
    auto dropdownArea = outer.removeFromTop (kDropdownRowH);
    layoutBootsyDropdownRow (dropdownArea,
                             rootNoteLabel, rootNoteBox,
                             scaleLabel, scaleBox,
                             octaveLabel, octaveBox,
                             styleLabel, styleBox);
    outer.removeFromTop (kSectionGap);

    // 2. Main area — piano roll + bass grid
    auto mainArea = outer.removeFromTop (kMainAreaH);
    {
        constexpr int pianoW = 56;
        auto gridRow = mainArea;
        pianoRoll.setBounds (gridRow.removeFromLeft (pianoW));
        gridRow.removeFromLeft (6);
        bassGrid.setBounds (gridRow);
    }
    outer.removeFromTop (kSectionGap);

    // 3. Bottom: knobs card (left) + loop/actions card (right)
    auto bottomArea = outer.removeFromTop (kBottomCardH);
    auto loopCard   = bottomArea.removeFromRight (kLoopCardW);
    bottomArea.removeFromRight (kCardGap);
    auto knobsCard  = bottomArea;

    // ── Knobs card: GROOVE row + EXPRESSION row ───────────────────────────
    {
        auto inner = knobsCard.reduced (14, 12);

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

        expressionLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        auto expression = inner.removeFromTop (halfH);
        {
            const int n = 5;
            const int gap = 6;
            const int kw = (expression.getWidth() - gap * (n - 1)) / n;
            knobFillRate.setBounds   (expression.removeFromLeft (kw)); expression.removeFromLeft (gap);
            knobComplexity.setBounds (expression.removeFromLeft (kw)); expression.removeFromLeft (gap);
            knobGhost.setBounds      (expression.removeFromLeft (kw)); expression.removeFromLeft (gap);
            knobStaccato.setBounds   (expression.removeFromLeft (kw));
            // one blank slot on the right; keeps columns aligned with the 5-knob row above
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

void BootsyPanel::paint (juce::Graphics& g)
{
    using namespace BootsyM3;
    using namespace bridge::instrumentLayout;
    g.fillAll (surface);

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

    const bool perfOn = proc.apvtsBootsy.getRawParameterValue ("perform")->load() > 0.5f;
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
        using namespace BootsyM3;
        performButton.setColour (juce::TextButton::buttonColourId,
                                 performButton.getToggleState() ? primary.withAlpha (0.35f)
                                                                : juce::Colours::transparentBlack);
        performButton.setColour (juce::TextButton::textColourOffId, BootsyColors::TextDim);
    }
}
