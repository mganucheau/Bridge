#include "DrumsPanel.h"
#include "BridgeAppleHIG.h"
#include "BridgeControlMetrics.h"
#include "BridgeLookAndFeel.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "InstrumentControlBar.h"
#include "MelodicGridLayout.h"
#include "drums/DrumsStylePresets.h"
#include "drums/DrumsLookAndFeel.h"

// ─── DrumsPanel ────────────────────────────────────────────────────────────────

DrumsPanel::DrumsPanel (BridgeProcessor& p)
    : proc (p),
      instrumentStrip (InstrumentControlBar::makeDrumsConfig (p)),
      midiClipEditor (p,
                      BridgeMidiClipEditor::InstrumentKind::drums,
                      bridge::colors::accentDrums(),
                      [this] (float dy) { adjustZoomX (dy); },
                      [this] (float dy) { adjustZoomY (dy); })
{
    setLookAndFeel (&laf);

    proc.apvtsMain.addParameterListener ("loopStart", this);
    proc.apvtsMain.addParameterListener ("loopEnd", this);
    proc.apvtsMain.addParameterListener ("playbackLoopOn", this);
    proc.apvtsMain.addParameterListener ("phraseBars", this);
    proc.apvtsMain.addParameterListener ("timeDivision", this);
    proc.apvtsDrums.addParameterListener ("tickerSpeed", this);
    proc.apvtsDrums.addParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "velocity", "fillRate", "complexity",
                            "hold", "ghostAmount", "intensity",
                            "lockKick", "lockSnare", "lockHats", "lockPerc",
                            "life", "hatOpen", "velShape" })
        proc.apvtsDrums.addParameterListener (id, this);
    proc.apvtsDrums.state.addListener (this);

    proc.apvtsMain.addParameterListener ("drumsOn", this);

    addAndMakeVisible (melodicViewport);
    melodicViewport.setViewedComponent (&midiClipEditor, false);
    melodicViewport.setScrollBarsShown (true, true);
    melodicViewport.setScrollBarThickness (10);
    melodicViewport.getVerticalScrollBar().setLookAndFeel (&laf);

    addAndMakeVisible (loopStrip);
    loopStrip.setStepLabelGutter ((int) bridge::kMelodicKeyStripWidth);
    addAndMakeVisible (instrumentStrip);

    // ── Knob column (SWING / HUMANIZE / HOLD) ─────────────────────────────
    const auto accent = bridge::colors::accentDrums();
    auto setupKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name,
                          const juce::String& paramId,
                          std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        BridgeLookAndFeel::setKnobStyle (s, BridgeLookAndFeel::KnobStyle::SmallLane, accent);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredTop);
        l.setColour (juce::Label::textColourId, bridge::colors::textDim());
        l.setFont (bridge::hig::uiFont (11.0f, "Semibold"));
        addAndMakeVisible (l);
        if (proc.apvtsDrums.getParameter (paramId) != nullptr)
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvtsDrums, paramId, s);
    };
    setupKnob (knobSwing,    labelSwing,    "SWING",    "swing",    attSwing);
    setupKnob (knobHumanize, labelHumanize, "HUMANIZE", "humanize", attHumanize);
    setupKnob (knobHold,     labelHold,     "HOLD",     "hold",     attHold);
    setupKnob (knobVelocity, labelVelocity, "VELOCITY", "velocity", attVelocity);

    // ── XY pads (PATTERN, FILLS) ──────────────────────────────────────────
    auto setupSectionLabel = [] (juce::Label& l, const juce::String& text,
                                  juce::Justification just = juce::Justification::centred)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (bridge::hig::uiFont (10.0f, "Bold"));
        l.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
        l.setJustificationType (just);
    };
    auto setupAxisLabel = [] (juce::Label& l, const juce::String& text, juce::Justification just)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (bridge::hig::uiFont (9.0f, "Semibold"));
        l.setColour (juce::Label::textColourId, bridge::colors::textDim());
        l.setJustificationType (just);
        l.setInterceptsMouseClicks (false, false);
    };

    patternPad = std::make_unique<BridgeXYPad> (proc.apvtsDrums, "complexity", "intensity", accent);
    fillsPad   = std::make_unique<BridgeXYPad> (proc.apvtsDrums, "fillRate",   "ghostAmount", accent);
    addAndMakeVisible (*patternPad);
    addAndMakeVisible (*fillsPad);

    setupSectionLabel (patternHeader, "PATTERN");
    setupSectionLabel (fillsHeader,   "FILLS");
    setupAxisLabel (patternXLabel, "COMPLEXITY \xe2\x86\x92", juce::Justification::centred);
    setupAxisLabel (patternYLabel, "DYN",                       juce::Justification::centred);
    setupAxisLabel (fillsXLabel,   "FILL RATE \xe2\x86\x92",    juce::Justification::centred);
    setupAxisLabel (fillsYLabel,   "DEPTH",                     juce::Justification::centred);
    addAndMakeVisible (patternHeader);
    addAndMakeVisible (fillsHeader);
    addAndMakeVisible (patternXLabel);
    addAndMakeVisible (patternYLabel);
    addAndMakeVisible (fillsXLabel);
    addAndMakeVisible (fillsYLabel);
    patternXLabel.setVisible (false);
    patternYLabel.setVisible (false);
    fillsXLabel.setVisible (false);
    fillsYLabel.setVisible (false);

    // XY pad readout knobs (same params as the pads).
    auto setupSmallKnob = [&] (juce::Slider& s, juce::Label& l,
                               const juce::String& labelText,
                               const juce::String& paramId,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        BridgeLookAndFeel::setKnobStyle (s, BridgeLookAndFeel::KnobStyle::SmallLane, accent);
        addAndMakeVisible (s);

        l.setText (labelText, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredTop);
        l.setColour (juce::Label::textColourId, bridge::colors::textDim());
        l.setFont (bridge::hig::uiFont (9.0f, "Semibold"));
        addAndMakeVisible (l);

        if (proc.apvtsDrums.getParameter (paramId) != nullptr)
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvtsDrums, paramId, s);
    };
    setupSmallKnob (patternKnobX, patternKnobXLabel, "COMPLEX", "complexity", attPatternKnobX);
    setupSmallKnob (patternKnobY, patternKnobYLabel, "DYN",     "intensity",  attPatternKnobY);
    setupSmallKnob (fillsKnobX,   fillsKnobXLabel,   "FILL",    "fillRate",   attFillsKnobX);
    setupSmallKnob (fillsKnobY,   fillsKnobYLabel,   "DEPTH",   "ghostAmount", attFillsKnobY);

    // ── Selectors + Actions (right column) ───────────────────────────────
    setupSectionLabel (selectorsHeader, "SELECTORS", juce::Justification::centredLeft);
    setupSectionLabel (actionsHeader,   "ACTIONS",   juce::Justification::centredLeft);
    addAndMakeVisible (selectorsHeader);
    addAndMakeVisible (actionsHeader);

    auto setupLoopKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name,
                              const juce::String& paramId,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        BridgeLookAndFeel::setKnobStyle (s, BridgeLookAndFeel::KnobStyle::SmallLane, accent);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredTop);
        l.setColour (juce::Label::textColourId, bridge::colors::textDim());
        l.setFont (bridge::hig::uiFont (10.0f, "Semibold"));
        addAndMakeVisible (l);
        if (proc.apvtsMain.getParameter (paramId) != nullptr)
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvtsMain, paramId, s);
    };
    setupLoopKnob (knobLoopStart, labelLoopStart, "START", "loopStart", attLoopStart);
    setupLoopKnob (knobLoopEnd,   labelLoopEnd,   "END",   "loopEnd",   attLoopEnd);

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
    addAndMakeVisible (loopPlaybackButton);
    if (proc.apvtsMain.getParameter ("playbackLoopOn") != nullptr)
        attLoopPlayback = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvtsMain, "playbackLoopOn", loopPlaybackButton);

    setupLoopStripToggle (syncIconButton, "spanLock", "S");
    syncIconButton.setTooltip ("Lock loop width: moving start or end keeps the same span");
    addAndMakeVisible (syncIconButton);
    if (proc.apvtsMain.getParameter ("loopSpanLock") != nullptr)
        attLoopSpan = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvtsMain, "loopSpanLock", syncIconButton);

    auto setupAction = [&] (juce::TextButton& b)
    {
        b.setConnectedEdges (0);
        b.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack());
        b.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim());
        addAndMakeVisible (b);
    };
    setupAction (generateButton);
    generateButton.onClick = [this] { proc.triggerDrumsGenerate(); };

    setupAction (jamToggle);
    jamToggle.setButtonText ("JAM");
    jamToggle.setClickingTogglesState (true);
    jamToggle.setTooltip ("Jam: periodically press Generate over the current selection (while playing)");
    jamToggle.setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.95f));
    jamToggle.setColour (juce::TextButton::buttonOnColourId, bridge::colors::knobTrack().brighter (0.12f));
    if (proc.apvtsDrums.getParameter ("jamOn") != nullptr)
        jamToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvtsDrums, "jamOn", jamToggle);

    bridge::phrase::stylePhraseLengthComboBox (jamPeriodBox);
    bridge::phrase::addPhraseLengthBarItems (jamPeriodBox);
    jamPeriodBox.setJustificationType (juce::Justification::centredLeft);
    jamPeriodBox.setTooltip ("Jam period (bars)");
    addAndMakeVisible (jamPeriodBox);
    if (proc.apvtsDrums.getParameter ("jamPeriodBars") != nullptr)
        jamPeriodAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvtsDrums, "jamPeriodBars", jamPeriodBox);
    jamLabel.setVisible (false);

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
    applyPhraseBars();
}

DrumsPanel::~DrumsPanel()
{
    densityComplexityDebounce.stopTimer();
    melodicViewport.getVerticalScrollBar().setLookAndFeel (nullptr);
    proc.apvtsMain.removeParameterListener ("drumsOn", this);
    proc.apvtsMain.removeParameterListener ("loopStart", this);
    proc.apvtsMain.removeParameterListener ("loopEnd", this);
    proc.apvtsMain.removeParameterListener ("playbackLoopOn", this);
    proc.apvtsMain.removeParameterListener ("phraseBars", this);
    proc.apvtsMain.removeParameterListener ("timeDivision", this);
    proc.apvtsDrums.removeParameterListener ("tickerSpeed", this);
    proc.apvtsDrums.removeParameterListener ("style", this);
    for (const char* id : { "density", "swing", "humanize", "velocity", "fillRate", "complexity",
                            "hold", "ghostAmount", "intensity",
                            "lockKick", "lockSnare", "lockHats", "lockPerc",
                            "life", "hatOpen", "velShape" })
        proc.apvtsDrums.removeParameterListener (id, this);
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

int DrumsPanel::currentPhraseBarCount() const
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("phraseBars")))
        return bridge::phrase::phraseBarsFromChoiceIndex (pc->getIndex());
    return bridge::phrase::phraseBarsFromChoiceIndex (0);
}

void DrumsPanel::applyPhraseBars()
{
    const int bars = currentPhraseBarCount();
    const int phraseSteps = bridge::phrase::phraseStepsForBars (bars);
    loopStrip.setBarRepeats (1);
    loopStrip.setNumSteps (phraseSteps);
    proc.drumEngine.setPhraseBars (bars);
    resized();
    repaint();
}

void DrumsPanel::applyDrumsPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("drumsOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("drumsOn")->load() > 0.5f;

    instrumentStrip.setTrackPowered (on);
    melodicViewport.setEnabled (on);
    midiClipEditor.setEnabled (on);
    loopStrip.setEnabled (on);

    auto enableAll = [on] (std::initializer_list<juce::Component*> cs)
    {
        for (auto* c : cs) if (c != nullptr) c->setEnabled (on);
    };
    enableAll ({ &knobSwing, &knobHumanize, &knobHold, &knobVelocity,
                 patternPad.get(), fillsPad.get(),
                 &knobLoopStart, &knobLoopEnd,
                 &loopPlaybackButton, &syncIconButton,
                 &generateButton, &jamToggle, &jamPeriodBox });

    const float dim = on ? 1.0f : 0.42f;
    melodicViewport.setAlpha (dim);
    midiClipEditor.setAlpha (dim);
    loopStrip.setAlpha (dim);
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

    melodicViewport.setBounds (card);
    const int viewW = juce::jmax (1, melodicViewport.getWidth());
    const int viewH = juce::jmax (1, melodicViewport.getHeight());
    const int phraseSteps = proc.drumEngine.getPatternLen();
    loopStrip.setNumSteps (phraseSteps);

    const int nRows = NUM_DRUMS;
    const int nSteps = phraseSteps;
    const int strip = (int) bridge::kMelodicKeyStripWidth;
    const float baseCellW = (float) (viewW - strip) / (float) juce::jmax (1, nSteps);
    const float baseCellH = (float) viewH / (float) nRows;
    const float cellW = baseCellW * zoomX;
    const float cellH = baseCellH * zoomY;
    const int bodyW = strip + (int) (cellW * (float) nSteps);
    const int chrome = BridgeMidiClipEditor::verticalChromePx;
    const int bodyH = chrome + juce::jmax (1, (int) (cellH * (float) nRows));
    midiClipEditor.setZoom (zoomX, zoomY);
    midiClipEditor.setSize (bodyW, bodyH);

    layoutBottomControls (shell.bottomStrip);
}

void DrumsPanel::adjustZoomX (float delta)
{
    zoomX = juce::jlimit (0.4f, 6.0f, zoomX * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void DrumsPanel::adjustZoomY (float delta)
{
    zoomY = juce::jlimit (0.4f, 6.0f, zoomY * (1.0f + delta * 0.3f));
    midiClipEditor.setZoom (zoomX, zoomY);
    resized();
}

void DrumsPanel::layoutBottomControls (juce::Rectangle<int> bottom)
{
    constexpr int smallGap = bridge::controlMetrics::kLayoutSmallGapPx;
    constexpr int kKnobW = bridge::controlMetrics::kKnobBodySidePx;
    constexpr int kKnobLabelH = bridge::controlMetrics::kKnobLabelBandHPx;
    constexpr int kLoopBtnSide = bridge::controlMetrics::kLoopSyncToggleSide;

    if (bottom.isEmpty())
        return;

    const int bottomHeight = bottom.getHeight();
    const int totalW       = bottom.getWidth();
    const int x0           = bottom.getX();
    const int y0           = bottom.getY();

    const int knobColWidth  = kKnobW * 2 + smallGap;
    const int rightColWidth = juce::jmax (knobColWidth, (int) std::round ((float) bottomHeight * 1.25f));
    const int minPad        = juce::jmax (1, (int) std::round ((float) bottomHeight * 0.85f));
    int padSize             = bottomHeight;
    {
        const int gaps = smallGap * 3;
        const int avail = totalW - knobColWidth - rightColWidth - gaps;
        if (padSize * 2 > avail)
            padSize = juce::jmax (minPad, avail / 2);
    }

    juce::Rectangle<int> knobCol (x0, y0, knobColWidth, bottomHeight);
    auto layKnobCell = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> cell)
    {
        auto labelArea = cell.removeFromBottom (kKnobLabelH);
        s.setBounds (cell.withSizeKeepingCentre (kKnobW, kKnobW));
        l.setBounds (labelArea);
    };
    {
        const int cellH = (bottomHeight - smallGap) / 2;
        auto topRow = knobCol.removeFromTop (cellH);
        knobCol.removeFromTop (smallGap);
        auto botRow = knobCol;
        const int cw = (knobColWidth - smallGap) / 2;
        layKnobCell (knobSwing, labelSwing, topRow.removeFromLeft (cw));
        topRow.removeFromLeft (smallGap);
        layKnobCell (knobHumanize, labelHumanize, topRow);
        layKnobCell (knobHold, labelHold, botRow.removeFromLeft (cw));
        botRow.removeFromLeft (smallGap);
        layKnobCell (knobVelocity, labelVelocity, botRow);
    }

    auto placePad = [&] (BridgeXYPad* pad, int colLeftX, juce::Label& header, juce::Label& xLab,
                          juce::Label& yLab)
    {
        if (pad == nullptr) return;
        constexpr int knobRowH = 54;
        constexpr int knobGap  = 6;
        const int spaceAboveReadouts = juce::jmax (1, bottomHeight - knobRowH - knobGap);
        const int rawSide = juce::jmax (bridge::controlMetrics::kKnobBodySidePx,
                                        juce::jmin (padSize, spaceAboveReadouts));
        const int side = juce::jmax (bridge::controlMetrics::kXyPadMinSidePx,
                                     (int) std::round ((float) rawSide * bridge::controlMetrics::kXyPadScaleOfMinDim));
        const int px   = colLeftX + (padSize - side) / 2;
        juce::Rectangle<int> padRect (px, y0, side, side);
        pad->setBounds (padRect);
        const int hdrH = juce::jmin (14, side / 5);
        header.setBounds (padRect.getX(), padRect.getY() + 2, padRect.getWidth(), hdrH);
        juce::ignoreUnused (xLab, yLab);
    };
    const int patternX = x0 + knobColWidth + smallGap;
    const int fillsX   = patternX + padSize + smallGap;
    placePad (patternPad.get(), patternX, patternHeader, patternXLabel, patternYLabel);
    placePad (fillsPad.get(),   fillsX,   fillsHeader,   fillsXLabel,   fillsYLabel);

    auto layoutTwoKnobsRow = [&] (int colLeftX, juce::Slider& a, juce::Label& aL, juce::Slider& b, juce::Label& bL)
    {
        constexpr int knobRowH = 54;
        constexpr int knobGap  = 6;
        const int spaceAboveReadouts = juce::jmax (1, bottomHeight - knobRowH - knobGap);
        const int rawSide = juce::jmax (bridge::controlMetrics::kKnobBodySidePx,
                                        juce::jmin (padSize, spaceAboveReadouts));
        const int side = juce::jmax (bridge::controlMetrics::kXyPadMinSidePx,
                                     (int) std::round ((float) rawSide * bridge::controlMetrics::kXyPadScaleOfMinDim));
        const int px   = colLeftX + (padSize - side) / 2;
        juce::Rectangle<int> padTop (px, y0, side, side);
        juce::Rectangle<int> below (colLeftX, padTop.getBottom() + knobGap, padSize, knobRowH);
        auto row = below;
        const int kw = (row.getWidth() - 8) / 2;
        auto left = row.removeFromLeft (kw);
        row.removeFromLeft (8);
        auto right = row;

        auto place = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> r)
        {
            l.setBounds (r.removeFromBottom (14));
            s.setBounds (r.withSizeKeepingCentre (kKnobW, kKnobW));
        };
        place (a, aL, left);
        place (b, bL, right);
    };
    layoutTwoKnobsRow (patternX, patternKnobX, patternKnobXLabel, patternKnobY, patternKnobYLabel);
    layoutTwoKnobsRow (fillsX,   fillsKnobX,   fillsKnobXLabel,   fillsKnobY,   fillsKnobYLabel);

    const int rightX = fillsX + padSize + smallGap;
    juce::Rectangle<int> rightCol (rightX, y0, juce::jmax (1, x0 + totalW - rightX), bottomHeight);

    const int halfH = (rightCol.getHeight() - 8) / 2;
    auto selArea = rightCol.removeFromTop (halfH);
    rightCol.removeFromTop (8);
    auto actArea = rightCol;

    selectorsHeader.setBounds (selArea.removeFromTop (12));
    selArea.removeFromTop (2);
    {
        constexpr int kBtnPairGap  = 4;
        constexpr int kOuterGap    = 8;
        const int kKnobH = kKnobLabelH + kKnobW;

        const int blockW = kKnobW + kOuterGap + kLoopBtnSide + kOuterGap + kKnobW;
        const int sx0    = selArea.getX() + (selArea.getWidth() - blockW) / 2;
        const int sy0    = selArea.getY() + (selArea.getHeight() - kKnobH) / 2;
        const int xBtn   = sx0 + kKnobW + kOuterGap;
        const int yTop   = selArea.getCentreY() - (kLoopBtnSide + kBtnPairGap + kLoopBtnSide) / 2;

        knobLoopStart.setBounds (sx0, sy0, kKnobW, kKnobW);
        labelLoopStart.setBounds (sx0, sy0 + kKnobW, kKnobW, kKnobLabelH);
        loopPlaybackButton.setBounds (xBtn, yTop, kLoopBtnSide, kLoopBtnSide);
        syncIconButton.setBounds (xBtn, yTop + kLoopBtnSide + kBtnPairGap, kLoopBtnSide, kLoopBtnSide);
        const int xEnd = sx0 + kKnobW + kOuterGap + kLoopBtnSide + kOuterGap;
        knobLoopEnd.setBounds (xEnd, sy0, kKnobW, kKnobW);
        labelLoopEnd.setBounds (xEnd, sy0 + kKnobW, kKnobW, kKnobLabelH);
    }

    actionsHeader.setBounds (actArea.removeFromTop (12));
    actArea.removeFromTop (2);
    {
        constexpr int kActionSide = bridge::controlMetrics::kKnobBodySidePx;
        constexpr int btnGap     = bridge::controlMetrics::kLayoutSmallGapPx;
        constexpr int outerMargin = 8;
        actArea.reduce (outerMargin, outerMargin);
        auto row = actArea;
        const int genY = row.getY() + (row.getHeight() - kActionSide) / 2;
        generateButton.setBounds (row.getX(), genY, kActionSide, kActionSide);
        jamToggle.setBounds (row.getX() + kActionSide + btnGap, genY, kActionSide, kActionSide);
        auto comboArea = row.withTrimmedLeft (kActionSide + btnGap + kActionSide + btnGap);
        const int comboH = juce::jmin (28, juce::jmax (22, row.getHeight() - 4));
        jamPeriodBox.setBounds (comboArea.withSizeKeepingCentre (comboArea.getWidth(), comboH));
    }
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
    midiClipEditor.repaint();
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
    if (parameterID == "phraseBars")
    {
        applyPhraseBars();
        return;
    }
    if (parameterID == "timeDivision")
    {
        midiClipEditor.repaint();
        return;
    }
    if (parameterID == "loopStart" || parameterID == "loopEnd" || parameterID == "playbackLoopOn"
        || parameterID == "uiTheme")
    {
        loopStrip.repaint();
        midiClipEditor.repaint();
        resized();
    }

    if (parameterID == "density" || parameterID == "complexity")
    {
        densityComplexityDebounce.startTimer (48);
        return;
    }
    if (parameterID == "style")
    {
        proc.syncDrumsEngineFromAPVTS();
        int si = 0;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsDrums.getParameter ("style")))
            si = c->getIndex();
        proc.drumEngine.adaptPatternToNewStyle (si);
        proc.rebuildDrumsGridPreview();
        triggerAsyncUpdate();
        return;
    }

    if (parameterID == "swing" || parameterID == "humanize" || parameterID == "velocity"
        || parameterID == "fillRate" || parameterID == "hold" || parameterID == "ghostAmount"
        || parameterID == "intensity" || parameterID == "tickerSpeed")
    {
        proc.rebuildDrumsGridPreview();
        triggerAsyncUpdate();
        return;
    }

    proc.rebuildDrumsGridPreview();
    triggerAsyncUpdate();
}

void DrumsPanel::DensityComplexityDebounce::timerCallback()
{
    stopTimer();
    p.performDebouncedDrumRegenerate();
}

void DrumsPanel::performDebouncedDrumRegenerate()
{
    proc.regenerateDrumsAfterKnobChange();
    triggerAsyncUpdate();
}

void DrumsPanel::updateStepAnimation()
{
    const int step = proc.drumsCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        midiClipEditor.updatePlayhead (step);
    }
}
