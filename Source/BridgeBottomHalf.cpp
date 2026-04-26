#include "BridgeBottomHalf.h"
#include "BridgeAppleHIG.h"
#include "BridgeInstrumentStripStyle.h"
#include "BridgePanelLayout.h"

namespace
{
static void setupStripLoopToggle (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setConnectedEdges (0);
    bridge::instrumentStripStyle::tagStripLoop (b);
    b.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

static void setupStripSpanLockToggle (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setConnectedEdges (0);
    bridge::instrumentStripStyle::tagStripSpanLock (b);
    b.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}
} // namespace

LabelledKnob::LabelledKnob (const juce::String& paramId, const juce::String& name,
                            juce::AudioProcessorValueTreeState& apvts, BridgeLookAndFeel::KnobStyle style, juce::Colour accent,
                            int rotaryDiameterIn,
                            int labelBandHeightIn)
    : rotaryDiameter (juce::jlimit (28, 72, rotaryDiameterIn)),
      labelBandHeight (juce::jlimit (10, 24, labelBandHeightIn))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    BridgeLookAndFeel::setKnobStyle (slider, style, accent);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredTop);
    label.setColour (juce::Label::textColourId, bridge::colors::textDim());
    label.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
    addAndMakeVisible (label);

    auto* param = apvts.getParameter (paramId);
    if (param != nullptr)
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (labelBandHeight));
    const int side = rotaryDiameter;
    slider.setBounds (b.withSizeKeepingCentre (side, side));
}

namespace
{
static void setupSectionLabel (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (bridge::hig::uiFont (10.0f, "Bold"));
    l.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
    l.setJustificationType (juce::Justification::centredLeft);
}
}

BridgeBottomHalf::BridgeBottomHalf (juce::AudioProcessorValueTreeState& instApvtsToUse,
                                    juce::AudioProcessorValueTreeState& mainApvtsToUse,
                                    BridgeLookAndFeel& laf,
                                    juce::Colour groupAccent,
                                    std::function<void()> onGenerate,
                                    std::function<void(bool)> onFillHold)
    : instApvts (instApvtsToUse),
      mainApvts (mainApvtsToUse),
      knobDensity ("density", "DENSITY", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobSwing ("swing", "SWING", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobHumanize ("humanize", "HUMANIZE", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobHold ("hold", "HOLD", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobVelocity ("velocity", "VELOCITY", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobFillRate ("fillRate", "FILL RATE", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobComplexity ("complexity", "COMPLEXITY", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobSustain ("sustain", "SUSTAIN", instApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      xyTension (std::make_unique<BridgeXYPad> (instApvts, "complexity", "density", groupAccent)),
      xyFeel (std::make_unique<BridgeXYPad> (instApvts, "humanize", "swing", groupAccent)),
      knobLoopStart ("loopStart", "START", mainApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      knobLoopEnd ("loopEnd", "END", mainApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent),
      fillHoldCallback (onFillHold)
{
    setLookAndFeel (&laf);

    setupSectionLabel (grooveLabel, "GROOVE");
    setupSectionLabel (expressionLabel, "EXPRESSION");
    setupSectionLabel (tensionLabel, "TENSION");
    setupSectionLabel (feelLabel, "FEEL");
    setupSectionLabel (selectorsLabel, "SELECTORS");
    setupSectionLabel (actionsLabel, "ACTIONS");

    addAndMakeVisible (grooveLabel);
    addAndMakeVisible (expressionLabel);
    addAndMakeVisible (tensionLabel);
    addAndMakeVisible (feelLabel);
    addAndMakeVisible (selectorsLabel);
    addAndMakeVisible (actionsLabel);
    addAndMakeVisible (knobDensity);
    addAndMakeVisible (knobSwing);
    addAndMakeVisible (knobHumanize);
    addAndMakeVisible (knobHold);
    addAndMakeVisible (knobVelocity);
    addAndMakeVisible (knobFillRate);
    addAndMakeVisible (knobComplexity);
    addAndMakeVisible (knobSustain);

    if (xyTension != nullptr)
        addAndMakeVisible (*xyTension);
    if (xyFeel != nullptr)
        addAndMakeVisible (*xyFeel);

    addAndMakeVisible (knobLoopStart);
    addAndMakeVisible (knobLoopEnd);

    setupStripLoopToggle (loopPlaybackButton);
    loopPlaybackButton.setTooltip ("Playback loop (wrap transport in selection)");
    loopPlaybackButton.setWantsKeyboardFocus (false);
    loopPlaybackButton.setMouseClickGrabsKeyboardFocus (false);
    if (mainApvts.getParameter ("playbackLoopOn") != nullptr)
        loopPlaybackAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mainApvts, "playbackLoopOn", loopPlaybackButton);
    addAndMakeVisible (loopPlaybackButton);

    setupStripSpanLockToggle (syncIconButton);
    syncIconButton.setTooltip ("Lock loop width: moving start or end keeps the same span");
    syncIconButton.setWantsKeyboardFocus (false);
    syncIconButton.setMouseClickGrabsKeyboardFocus (false);
    if (mainApvts.getParameter ("loopSpanLock") != nullptr)
        syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mainApvts, "loopSpanLock", syncIconButton);
    addAndMakeVisible (syncIconButton);

    generateButton.onClick = onGenerate;

    auto setupAction = [&] (juce::TextButton& btn) {
        btn.setConnectedEdges (0);
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack());
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim());
        addAndMakeVisible (btn);
    };
    setupAction (generateButton);

    jamToggle.setButtonText ("JAM");
    jamToggle.setTooltip ("Jam: periodically press Generate over the current selection (while playing)");
    addAndMakeVisible (jamToggle);
    if (instApvts.getParameter ("jamOn") != nullptr)
        jamToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            instApvts, "jamOn", jamToggle);

    jamPeriodBox.addItem ("4 bars",  1);
    jamPeriodBox.addItem ("8 bars",  2);
    jamPeriodBox.addItem ("16 bars", 3);
    jamPeriodBox.setJustificationType (juce::Justification::centredLeft);
    jamPeriodBox.setTooltip ("Jam period (bars)");
    addAndMakeVisible (jamPeriodBox);
    if (instApvts.getParameter ("jamPeriodBars") != nullptr)
        jamPeriodAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            instApvts, "jamPeriodBars", jamPeriodBox);

    jamLabel.setText ("JAM", juce::dontSendNotification);
    jamLabel.setJustificationType (juce::Justification::centred);
    jamLabel.setColour (juce::Label::textColourId, bridge::colors::textDim());
    jamLabel.setFont (bridge::hig::uiFont (10.0f, "Semibold"));
    addAndMakeVisible (jamLabel);
}

BridgeBottomHalf::~BridgeBottomHalf()
{
    setLookAndFeel (nullptr);
}

void BridgeBottomHalf::paint (juce::Graphics& g)
{
    auto rf = getLocalBounds().toFloat();
    g.setColour (bridge::colors::cardSurface());
    g.fillRect (rf);
    g.setColour (bridge::colors::cardOutline().withAlpha (0.35f));
    g.drawRect (rf.reduced (0.5f), 1.0f);

    constexpr int kGap = 12;
    constexpr int kMargin = 16;
    const int nCols = 4;
    const int innerW = getWidth() - kMargin * 2 - kGap * (nCols - 1);
    const int cw = innerW / nCols;
    int x = kMargin + cw;
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (bridge::colors::cardOutline().withAlpha (0.45f));
        g.drawVerticalLine (x, rf.getY() + 12.0f, rf.getBottom() - 12.0f);
        x += cw + kGap;
    }
}

void BridgeBottomHalf::resized()
{
    constexpr int kColGap = 12;
    constexpr int kTopGap = 12;
    constexpr int kMargin = 16;
    constexpr int kLoopBtnSide = 48;
    constexpr int kBtnPairGap = 4;
    constexpr int kOuterGap = 8;
    constexpr int kKnobW = 48;
    constexpr int kKnobLabelH = 16;
    constexpr int kKnobH = kKnobLabelH + kKnobW;

    auto area = getLocalBounds().reduced (kMargin, kTopGap);
    const int totalW = area.getWidth();
    const int innerW = totalW - kColGap * 3;
    const int cw = innerW / 4;

    auto col1 = area.removeFromLeft (cw);
    area.removeFromLeft (kColGap);
    auto col2 = area.removeFromLeft (cw);
    area.removeFromLeft (kColGap);
    auto col3 = area.removeFromLeft (cw);
    area.removeFromLeft (kColGap);
    auto col4 = area;

    // Col 1: Groove (hold / velocity) + Expression — no duplicates of Tension/Feel XY
    grooveLabel.setBounds (col1.removeFromTop (14));
    col1.removeFromTop (4);
    {
        auto row = col1.removeFromTop (70);
        const int kGap = 8;
        const int kw = (row.getWidth() - kGap) / 2;
        knobHold.setBounds (row.removeFromLeft (kw));
        row.removeFromLeft (kGap);
        knobVelocity.setBounds (row);
    }
    col1.removeFromTop (6);
    expressionLabel.setBounds (col1.removeFromTop (14));
    col1.removeFromTop (4);
    {
        auto row = col1.removeFromTop (70);
        const int kGap = 8;
        const bool haveSustain = instApvts.getParameter ("sustain") != nullptr;
        knobSustain.setVisible (haveSustain);
        if (haveSustain)
        {
            const int kw = (row.getWidth() - kGap) / 2;
            knobFillRate.setBounds (row.removeFromLeft (kw));
            row.removeFromLeft (kGap);
            knobSustain.setBounds (row);
        }
        else
        {
            knobFillRate.setBounds (row);
        }
    }

    // Col 2: Tension XY + per-axis knobs (same params).
    knobComplexity.setVisible (true);
    knobDensity.setVisible (true);
    tensionLabel.setBounds (col2.removeFromTop (14));
    col2.removeFromTop (4);
    {
        auto knobsRow = col2.removeFromBottom (70);
        const int kGap = 8;
        const int kw = (knobsRow.getWidth() - kGap) / 2;
        knobComplexity.setBounds (knobsRow.removeFromLeft (kw));
        knobsRow.removeFromLeft (kGap);
        knobDensity.setBounds (knobsRow);

        if (xyTension != nullptr)
        {
            const int padSide = juce::jmin (col2.getWidth(), col2.getHeight());
            xyTension->setBounds (col2.withSizeKeepingCentre (padSide, padSide));
        }
    }

    // Col 3: Feel XY + per-axis knobs (same params).
    knobHumanize.setVisible (true);
    knobSwing.setVisible (true);
    feelLabel.setBounds (col3.removeFromTop (14));
    col3.removeFromTop (4);
    {
        auto knobsRow = col3.removeFromBottom (70);
        const int kGap = 8;
        const int kw = (knobsRow.getWidth() - kGap) / 2;
        knobHumanize.setBounds (knobsRow.removeFromLeft (kw));
        knobsRow.removeFromLeft (kGap);
        knobSwing.setBounds (knobsRow);

        if (xyFeel != nullptr)
        {
            const int padSide = juce::jmin (col3.getWidth(), col3.getHeight());
            xyFeel->setBounds (col3.withSizeKeepingCentre (padSide, padSide));
        }
    }

    // Col 4: Selectors + Actions
    const int selectorsH = (col4.getHeight() - 8 - 8) / 2;
    auto selArea = col4.removeFromTop (selectorsH);
    col4.removeFromTop (8);
    auto actArea = col4;

    selectorsLabel.setBounds (selArea.removeFromTop (14));
    selArea.removeFromTop (4);
    {
        const int midW = kLoopBtnSide;
        const int blockW = kKnobW + kOuterGap + midW + kOuterGap + kKnobW;
        const int x0 = selArea.getX() + (selArea.getWidth() - blockW) / 2;
        const int y0 = selArea.getY() + (selArea.getHeight() - kKnobH) / 2;
        const int stackH = kLoopBtnSide + kBtnPairGap + kLoopBtnSide;
        const int xBtn   = x0 + kKnobW + kOuterGap;
        const int yTop   = selArea.getCentreY() - stackH / 2;
        knobLoopStart.setBounds (x0, y0, kKnobW, kKnobH);
        loopPlaybackButton.setBounds (xBtn, yTop, kLoopBtnSide, kLoopBtnSide);
        syncIconButton.setBounds (xBtn, yTop + kLoopBtnSide + kBtnPairGap, kLoopBtnSide, kLoopBtnSide);
        knobLoopEnd.setBounds (x0 + kKnobW + kOuterGap + midW + kOuterGap, y0, kKnobW, kKnobH);
    }

    actionsLabel.setBounds (actArea.removeFromTop (14));
    actArea.removeFromTop (4);
    {
        constexpr int kActionSide = 48;
        constexpr int btnGap = 6;
        constexpr int outerMargin = 8;
        actArea.reduce (outerMargin, outerMargin);
        auto row = actArea;
        const int genY = row.getY() + (row.getHeight() - kActionSide) / 2;
        generateButton.setBounds (row.getX(), genY, kActionSide, kActionSide);
        auto jamCol = row.withTrimmedLeft (kActionSide + btnGap);
        jamLabel.setBounds (jamCol.removeFromBottom (14));
        auto top = jamCol.removeFromTop (juce::jmax (18, jamCol.getHeight() / 2));
        jamToggle.setBounds (top.reduced (2, 0));
        jamPeriodBox.setBounds (jamCol.reduced (2, 0));
    }
}
