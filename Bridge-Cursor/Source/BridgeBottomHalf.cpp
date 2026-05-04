#include "BridgeBottomHalf.h"
#include "BridgeAppleHIG.h"
#include "BridgeControlMetrics.h"
#include "BridgeInstrumentStripStyle.h"
#include "BridgePanelLayout.h"
#include "BridgePhrase.h"

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
      knobLoopStart ("loopStart", "START", mainApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent,
                      bridge::controlMetrics::kKnobBodySidePx, bridge::controlMetrics::kKnobLabelBandHPx),
      knobLoopEnd ("loopEnd", "END", mainApvts, BridgeLookAndFeel::KnobStyle::SmallLane, groupAccent,
                    bridge::controlMetrics::kKnobBodySidePx, bridge::controlMetrics::kKnobLabelBandHPx),
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

    {
        auto setupLoopStripToggle = [] (juce::TextButton& b, const juce::String& stripTag, const juce::String& letter)
        {
            b.setButtonText (letter);
            b.setClickingTogglesState (true);
            b.setConnectedEdges (0);
            b.getProperties().set ("bridgeStripTag", stripTag);
            b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        };
        setupLoopStripToggle (loopPlaybackButton, "loop", "L");
        loopPlaybackButton.setTooltip ("Playback loop (wrap transport in selection)");
        loopPlaybackButton.setWantsKeyboardFocus (false);
        loopPlaybackButton.setMouseClickGrabsKeyboardFocus (false);
        if (mainApvts.getParameter ("playbackLoopOn") != nullptr)
            loopPlaybackAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                mainApvts, "playbackLoopOn", loopPlaybackButton);
        addAndMakeVisible (loopPlaybackButton);

        setupLoopStripToggle (syncIconButton, "spanLock", "S");
        syncIconButton.setTooltip ("Lock loop width: moving start or end keeps the same span");
        syncIconButton.setWantsKeyboardFocus (false);
        syncIconButton.setMouseClickGrabsKeyboardFocus (false);
        if (mainApvts.getParameter ("loopSpanLock") != nullptr)
            syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                mainApvts, "loopSpanLock", syncIconButton);
        addAndMakeVisible (syncIconButton);
    }

    generateButton.onClick = onGenerate;

    auto setupAction = [&] (juce::TextButton& btn) {
        btn.setConnectedEdges (0);
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack());
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim());
        addAndMakeVisible (btn);
    };
    setupAction (generateButton);

    setupAction (jamToggle);
    jamToggle.setButtonText ("JAM");
    jamToggle.setClickingTogglesState (true);
    jamToggle.setTooltip ("Jam: periodically press Generate over the current selection (while playing)");
    jamToggle.setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.95f));
    jamToggle.setColour (juce::TextButton::buttonOnColourId, bridge::colors::knobTrack().brighter (0.12f));
    if (instApvts.getParameter ("jamOn") != nullptr)
        jamToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            instApvts, "jamOn", jamToggle);

    bridge::phrase::stylePhraseLengthComboBox (jamPeriodBox);
    bridge::phrase::addPhraseLengthBarItems (jamPeriodBox);
    jamPeriodBox.setJustificationType (juce::Justification::centredLeft);
    jamPeriodBox.setTooltip ("Jam period (bars)");
    addAndMakeVisible (jamPeriodBox);
    if (instApvts.getParameter ("jamPeriodBars") != nullptr)
        jamPeriodAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            instApvts, "jamPeriodBars", jamPeriodBox);

    jamLabel.setVisible (false);

    hasRollSpanParam = (instApvts.getParameter ("rollSpanOctaves") != nullptr);
    if (hasRollSpanParam)
    {
        rollSpanOctavesBox.addItem ("1 oct", 1);
        rollSpanOctavesBox.addItem ("2 oct", 2);
        bridge::phrase::stylePhraseLengthComboBox (rollSpanOctavesBox);
        rollSpanOctavesBox.setJustificationType (juce::Justification::centredLeft);
        rollSpanOctavesBox.setTooltip ("Visible pitch span in the piano roll");
        addAndMakeVisible (rollSpanOctavesBox);
        rollSpanOctavesAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            instApvts, "rollSpanOctaves", rollSpanOctavesBox);
    }
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
    constexpr int kLoopBtnSide = bridge::controlMetrics::kLoopSyncToggleSide;
    constexpr int kBtnPairGap = 4;
    constexpr int kOuterGap = 8;
    constexpr int kKnobW = bridge::controlMetrics::kKnobBodySidePx;
    constexpr int kKnobLabelH = 16;
    constexpr int kKnobH = kKnobLabelH + kKnobW;
    constexpr int kLoopKnobH = bridge::controlMetrics::kKnobLabelBandHPx + bridge::controlMetrics::kKnobBodySidePx;

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
    grooveLabel.setBounds (col1.removeFromTop (12));
    col1.removeFromTop (2);
    {
        auto row = col1.removeFromTop (70);
        const int kGap = 8;
        const int kw = (row.getWidth() - kGap) / 2;
        knobHold.setBounds (row.removeFromLeft (kw));
        row.removeFromLeft (kGap);
        knobVelocity.setBounds (row);
    }
    col1.removeFromTop (6);
    expressionLabel.setBounds (col1.removeFromTop (12));
    col1.removeFromTop (2);
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
    tensionLabel.setBounds (col2.removeFromTop (12));
    col2.removeFromTop (2);
    {
        auto knobsRow = col2.removeFromBottom (70);
        const int kGap = 8;
        const int kw = (knobsRow.getWidth() - kGap) / 2;
        knobComplexity.setBounds (knobsRow.removeFromLeft (kw));
        knobsRow.removeFromLeft (kGap);
        knobDensity.setBounds (knobsRow);

        if (xyTension != nullptr)
        {
            const int raw = juce::jmin (col2.getWidth(), col2.getHeight());
            const int padSide = juce::jmax (bridge::controlMetrics::kXyPadMinSidePx,
                                            (int) std::round ((float) raw * bridge::controlMetrics::kXyPadScaleOfMinDim));
            xyTension->setBounds (col2.withSizeKeepingCentre (padSide, padSide));
        }
    }

    // Col 3: Feel XY + per-axis knobs (same params).
    knobHumanize.setVisible (true);
    knobSwing.setVisible (true);
    feelLabel.setBounds (col3.removeFromTop (12));
    col3.removeFromTop (2);
    {
        auto knobsRow = col3.removeFromBottom (70);
        const int kGap = 8;
        const int kw = (knobsRow.getWidth() - kGap) / 2;
        knobHumanize.setBounds (knobsRow.removeFromLeft (kw));
        knobsRow.removeFromLeft (kGap);
        knobSwing.setBounds (knobsRow);

        if (xyFeel != nullptr)
        {
            const int raw = juce::jmin (col3.getWidth(), col3.getHeight());
            const int padSide = juce::jmax (bridge::controlMetrics::kXyPadMinSidePx,
                                            (int) std::round ((float) raw * bridge::controlMetrics::kXyPadScaleOfMinDim));
            xyFeel->setBounds (col3.withSizeKeepingCentre (padSide, padSide));
        }
    }

    // Col 4: Selectors + Actions
    const int selectorsH = (col4.getHeight() - 8 - 8) / 2;
    auto selArea = col4.removeFromTop (selectorsH);
    col4.removeFromTop (8);
    auto actArea = col4;

    selectorsLabel.setBounds (selArea.removeFromTop (12));
    selArea.removeFromTop (2);
    {
        const int midW = kLoopBtnSide;
        const int blockW = kKnobW + kOuterGap + midW + kOuterGap + kKnobW;
        const int x0 = selArea.getX() + (selArea.getWidth() - blockW) / 2;
        const int y0 = selArea.getY() + (selArea.getHeight() - kLoopKnobH) / 2;
        const int stackH = kLoopBtnSide + kBtnPairGap + kLoopBtnSide;
        const int xBtn   = x0 + kKnobW + kOuterGap;
        const int yTop   = selArea.getCentreY() - stackH / 2;
        knobLoopStart.setBounds (x0, y0, kKnobW, kLoopKnobH);
        loopPlaybackButton.setBounds (xBtn, yTop, kLoopBtnSide, kLoopBtnSide);
        syncIconButton.setBounds (xBtn, yTop + kLoopBtnSide + kBtnPairGap, kLoopBtnSide, kLoopBtnSide);
        knobLoopEnd.setBounds (x0 + kKnobW + kOuterGap + midW + kOuterGap, y0, kKnobW, kLoopKnobH);
    }

    actionsLabel.setBounds (actArea.removeFromTop (12));
    actArea.removeFromTop (2);
    {
        constexpr int kActionSide = bridge::controlMetrics::kKnobBodySidePx;
        constexpr int btnGap = bridge::controlMetrics::kLayoutSmallGapPx;
        constexpr int outerMargin = 8;
        actArea.reduce (outerMargin, outerMargin);
        auto row = actArea;
        const int genY = row.getY() + (row.getHeight() - kActionSide) / 2;
        generateButton.setBounds (row.getX(), genY, kActionSide, kActionSide);
        jamToggle.setBounds (row.getX() + kActionSide + btnGap, genY, kActionSide, kActionSide);
        auto comboArea = row.withTrimmedLeft (kActionSide + btnGap + kActionSide + btnGap);
        constexpr int comboStackGap = 6;
        if (hasRollSpanParam)
        {
            const int stackH = juce::jmax (1, comboArea.getHeight() - comboStackGap);
            const int slotH  = stackH / 2;
            const int y0     = comboArea.getY() + (comboArea.getHeight() - (slotH * 2 + comboStackGap)) / 2;
            jamPeriodBox.setBounds (comboArea.getX(), y0, comboArea.getWidth(), slotH);
            rollSpanOctavesBox.setBounds (comboArea.getX(), y0 + slotH + comboStackGap, comboArea.getWidth(), slotH);
        }
        else
        {
            const int comboH = juce::jmin (28, juce::jmax (22, comboArea.getHeight() - 4));
            jamPeriodBox.setBounds (comboArea.withSizeKeepingCentre (comboArea.getWidth(), comboH));
        }
    }
}
