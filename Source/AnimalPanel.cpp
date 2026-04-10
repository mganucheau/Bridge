#include "AnimalPanel.h"

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

    constexpr float margin   = 10.0f;
    constexpr float headerH  = 26.0f;
    auto inner = full.reduced (margin);

    float hBody = inner.getHeight() - headerH;
    float gridTop = inner.getY() + headerH;

    constexpr float labelW = 72.0f;
    constexpr float buttonW = 22.0f;
    constexpr float buttonGap = 3.0f;
    constexpr float labelPadRight = 10.0f;
    const float gridX = inner.getX() + labelW + labelPadRight + (buttonW * 2.0f) + (buttonGap * 2.0f) + 8.0f;

    float cellW = (inner.getRight() - gridX) / (float)NUM_STEPS;
    float cellH = hBody / (float)NUM_DRUMS;
    float pad   = 2.0f;

    g.setColour (onSurfaceVariant);
    g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        float cx = gridX + step * cellW;
        auto cell = juce::Rectangle<float> (cx, inner.getY(), cellW, headerH - 2.0f);
        g.drawText (juce::String (step + 1), cell, juce::Justification::centred, false);
    }

    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Semibold")));
    g.drawText ("Lane", juce::Rectangle<float> (inner.getX(), inner.getY(), labelW, headerH - 2.0f),
                juce::Justification::centredRight, false);

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        float cy = gridTop + visualRow * cellH;

        const bool muted = proc.apvtsAnimal.getRawParameterValue ("mute_" + juce::String (drum))->load() > 0.5f;
        const bool soloed = proc.apvtsAnimal.getRawParameterValue ("solo_" + juce::String (drum))->load() > 0.5f;
        bool anySolo = false;
        for (int d = 0; d < NUM_DRUMS; ++d)
            anySolo = anySolo || (proc.apvtsAnimal.getRawParameterValue ("solo_" + juce::String (d))->load() > 0.5f);

        const bool audible = anySolo ? soloed : ! muted;

        g.setColour (audible ? onSurfaceVariant : onSurfaceVariant.withAlpha (0.35f));
        g.setFont   (juce::Font (juce::FontOptions().withHeight (13.0f)));
        g.drawText  (DRUM_NAMES[drum],
                     (int) inner.getX(), (int) cy, (int) labelW, (int) cellH,
                     juce::Justification::centredRight, false);

        for (int step = 0; step < NUM_STEPS; ++step)
        {
            float cx = gridX + step * cellW + pad;
            auto  cell = juce::Rectangle<float> (cx, cy + pad,
                                                 cellW - pad * 2, cellH - pad * 2);

            bool inPattern = (step < patLen);
            bool inLoop = inPattern && (step >= ls0 && step <= le0);
            const auto& hit = pattern[step][drum];

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
        float cx = gridX + currentStep * cellW;
        g.setColour (primary.withAlpha (0.08f));
        g.fillRect  (cx, gridTop, cellW, hBody);

        g.setColour (primary.withAlpha (0.85f));
        g.fillRect  (cx + pad, inner.getY(), cellW - pad * 2, 2.0f);
    }
}

void DrumGridComponent::mouseDown (const juce::MouseEvent& e)
{
    int   patLen = proc.drumEngine.getPatternLen();

    int loopStart = 1, loopEnd = NUM_STEPS;
    proc.getAnimalLoopBounds (loopStart, loopEnd);

    constexpr float margin   = 10.0f;
    constexpr float headerH  = 26.0f;
    constexpr float labelW = 72.0f;
    constexpr float buttonW = 22.0f;
    constexpr float buttonGap = 3.0f;
    constexpr float labelPadRight = 10.0f;
    const float innerLeft = margin;
    const float innerRight = (float) getWidth() - margin;
    const float gridX = innerLeft + labelW + labelPadRight + (buttonW * 2.0f) + (buttonGap * 2.0f) + 8.0f;

    float hBody = (float) getHeight() - margin * 2.0f - headerH;
    float gridTop = margin + headerH;

    float cellW  = (innerRight - gridX) / (float) NUM_STEPS;
    float cellH  = hBody / (float)NUM_DRUMS;

    int step = (int) ((e.x - gridX) / cellW);
    int visualRow = (int) ((e.y - gridTop) / cellH);
    int drum = visualRowToDrum (visualRow);

    if (e.y < gridTop) return;

    if (step < 0 || step >= patLen || drum < 0 || drum >= NUM_DRUMS) return;

    auto& pat = const_cast<DrumPattern&>(proc.drumEngine.getPattern());
    pat[step][drum].active   = !pat[step][drum].active;
    pat[step][drum].velocity = 100;

    juce::ignoreUnused (loopStart, loopEnd);
    repaint();
}

void DrumGridComponent::resized()
{
    constexpr int labelW = 72;
    constexpr int labelPad = 10;
    constexpr int buttonW = 22;
    constexpr int buttonGap = 3;
    constexpr int margin = 10;
    constexpr int headerH = 26;

    auto r = getLocalBounds().reduced (margin);
    r.removeFromTop (headerH);
    const int rowH = r.getHeight() / NUM_DRUMS;

    for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
    {
        const int drum = visualRowToDrum (visualRow);
        auto row = juce::Rectangle<int> (r.getX(), r.getY() + visualRow * rowH, r.getWidth(), rowH);
        auto controls = row.removeFromLeft (labelW + labelPad + buttonW * 2 + buttonGap * 2 + 2);
        controls.removeFromLeft (labelW + labelPad);

        auto mute = controls.removeFromLeft (buttonW);
        controls.removeFromLeft (buttonGap);
        auto solo = controls.removeFromLeft (buttonW);

        muteButtons[drum]->setBounds (mute.reduced (0, 3));
        soloButtons[drum]->setBounds (solo.reduced (0, 3));
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

    proc.apvtsAnimal.state.addListener (this);

    addAndMakeVisible (drumGrid);

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

    for (int i = 0; i < NUM_STYLES; ++i)
        styleBox.addItem (STYLE_NAMES[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsAnimal, "style", styleBox);

    generateButton.setTooltip ("Randomize settings and generate a new pattern.");
    generateButton.onClick = [this] { proc.triggerAnimalGenerate(); };
    generateButton.setButtonText ("GEN");

    performButton.setTooltip ("While transport runs: regenerate with seamless crossfade into the next loop.");
    performButton.setButtonText ("PERF");
    performButton.setClickingTogglesState (true);
    performAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (proc.apvtsAnimal, "perform", performButton);

    fillButton.setTooltip ("Hold to layer live fills on top of the current groove.");
    fillButton.setButtonText ("FILL");
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
    auto area = getLocalBounds().reduced (16);

    area.removeFromTop (8);

    const int bottomPanelH = 332;
    auto bottom = area.removeFromBottom (bottomPanelH);
    drumGrid.setBounds (area);

    bottom.removeFromTop (6);

    auto knobRow1 = bottom.removeFromTop (96);
    int knobW = juce::jmax (68, knobRow1.getWidth() / 4);
    knobDensity.setBounds    (knobRow1.removeFromLeft (knobW));
    knobSwing.setBounds      (knobRow1.removeFromLeft (knobW));
    knobHumanize.setBounds   (knobRow1.removeFromLeft (knobW));
    knobPocket.setBounds     (knobRow1);

    bottom.removeFromTop (4);
    auto knobRow2 = bottom.removeFromTop (96);
    knobW = juce::jmax (68, knobRow2.getWidth() / 4);
    knobVelocity.setBounds   (knobRow2.removeFromLeft (knobW));
    knobFillRate.setBounds   (knobRow2.removeFromLeft (knobW));
    knobComplexity.setBounds (knobRow2.removeFromLeft (knobW));
    knobGhost.setBounds      (knobRow2);

    bottom.removeFromTop (8);

    auto loopRow = bottom.removeFromTop (100);
    loopSectionLabel.setBounds (loopRow.removeFromLeft (40).withHeight (16));
    knobLoopStart.setBounds (loopRow.removeFromLeft (88));
    loopWidthLockButton.setBounds (loopRow.removeFromLeft (32).withSizeKeepingCentre (28, 28));
    knobLoopEnd.setBounds (loopRow.removeFromLeft (88));
    loopRow.removeFromLeft (8);

    tickerLabel.setBounds (loopRow.removeFromLeft (44).withHeight (18));
    auto tickRow = loopRow.removeFromLeft (92);
    const int th = juce::jmin (24, tickRow.getHeight());
    tickerFastButton.setBounds   (tickRow.removeFromLeft (30).withHeight (th).reduced (0, 2));
    tickerNormalButton.setBounds (tickRow.removeFromLeft (30).withHeight (th).reduced (0, 2));
    tickerSlowButton.setBounds   (tickRow.removeFromLeft (30).withHeight (th).reduced (0, 2));

    loopRow.removeFromLeft (10);
    const int actW = juce::jmin (56, loopRow.getWidth() / 3);
    generateButton.setBounds (loopRow.removeFromLeft (actW).reduced (0, 10));
    performButton.setBounds  (loopRow.removeFromLeft (actW).reduced (0, 10));
    fillButton.setBounds     (loopRow.removeFromLeft (actW).reduced (0, 10));

    bottom.removeFromTop (6);

    auto styleRow = bottom.removeFromTop (36);
    styleLabel.setBounds (styleRow.removeFromLeft (48).reduced (0, 6));
    styleBox.setBounds (styleRow.removeFromLeft (juce::jmin (420, styleRow.getWidth())).reduced (0, 4));
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
}
