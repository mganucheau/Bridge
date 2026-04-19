#include "MainPanel.h"
#include "MelodicGridLayout.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "BridgeInstrumentUI.h"
#include "drums/DrumsStylePresets.h"
#include "bass/BassStylePresets.h"

MainPanel::MainPanel (BridgeProcessor& p)
    : proc (p),
      bottomHalf (p.apvtsMain, laf, bridge::colors::accentLeader,
                  [&p] { p.triggerDrumsGenerate(); p.triggerBassGenerate(); p.triggerPianoGenerate(); p.triggerGuitarGenerate(); },
                  [&p] (bool active) { 
                      p.drumEngine.setFillHoldActive(active); 
                      p.bassEngine.setFillHoldActive(active); 
                      p.pianoEngine.setFillHoldActive(active); 
                      p.guitarEngine.setFillHoldActive(active); 
                  }) // Leader Fill Hold (macro?) // TODO implementation
{
    setLookAndFeel (&laf);

    proc.apvtsMain.state.addListener (this);

    auto setupCombo = [&] (juce::Label& lbl, juce::ComboBox& box, const juce::String& title,
                           juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
    {
        lbl.setText (title, juce::dontSendNotification);
        lbl.setColour (juce::Label::textColourId, bridge::colors::textSecondary);
        addAndMakeVisible (lbl);
        addAndMakeVisible (box);
        if (apvts.getParameter(paramId))
        {
            if (title == "STYLE") styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
            if (title == "ROOT") rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
            if (title == "SCALE") scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
            if (title == "OCT") octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
        }
    };

    setupCombo (styleLabel, styleBox, "STYLE", proc.apvtsMain, "leaderStyle");
    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);

    setupCombo (rootLabel, rootBox, "ROOT", proc.apvtsMain, "rootNote");
    static const char* roots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) rootBox.addItem (roots[i], i + 1);

    setupCombo (scaleLabel, scaleBox, "SCALE", proc.apvtsMain, "scale");
    for (int i = 0; i < BassPreset::NUM_SCALES; ++i) scaleBox.addItem (BassPreset::SCALE_NAMES[i], i + 1);

    setupCombo (octaveLabel, octaveBox, "OCT", proc.apvtsMain, "octave");
    for (int i = 1; i <= 4; ++i) octaveBox.addItem (juce::String (i), i);

    setupRow (drumsRow,  "DRUMS",  StripPreview::Kind::drums,  "mainMuteDrums",  "mainSoloDrums",  bridge::colors::accentDrums);
    setupRow (bassRow,   "BASS",   StripPreview::Kind::bass,   "mainMuteBass",   "mainSoloBass",   bridge::colors::accentBass);
    setupRow (pianoRow,  "PIANO",  StripPreview::Kind::piano,  "mainMutePiano",  "mainSoloPiano",  bridge::colors::accentPiano);
    setupRow (guitarRow, "GUITAR", StripPreview::Kind::guitar, "mainMuteGuitar", "mainSoloGuitar", bridge::colors::accentGuitar);

    addAndMakeVisible (bottomHalf);
    addAndMakeVisible (pagePower);
    powerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, "leaderTabOn", pagePower);
    proc.apvtsMain.addParameterListener ("leaderTabOn", this);

    startTimerHz (30);
    syncSoloButtonColours();
    applyLeaderPageState();
}

MainPanel::~MainPanel()
{
    proc.apvtsMain.removeParameterListener ("leaderTabOn", this);
    proc.apvtsMain.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void MainPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "leaderTabOn")
        applyLeaderPageState();
}

void MainPanel::applyLeaderPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("leaderTabOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("leaderTabOn")->load() > 0.5f;

    bottomHalf.setEnabled (on);
    styleLabel.setEnabled (on);
    styleBox.setEnabled (on);
    rootLabel.setEnabled (on);
    rootBox.setEnabled (on);
    scaleLabel.setEnabled (on);
    scaleBox.setEnabled (on);
    octaveLabel.setEnabled (on);
    octaveBox.setEnabled (on);

    pagePower.setEnabled (true);
    pagePower.setToggleState (on, juce::dontSendNotification);
    pagePower.setAlpha (1.0f);

    const float dim = on ? 1.0f : 0.42f;
    auto dimRow = [&] (Row& row)
    {
        row.name.setEnabled (on);
        row.mute.setEnabled (on);
        row.solo.setEnabled (on);
        if (row.preview != nullptr)
        {
            row.preview->setEnabled (on);
            row.preview->setAlpha (dim);
        }
    };
    dimRow (drumsRow);
    dimRow (bassRow);
    dimRow (pianoRow);
    dimRow (guitarRow);
}

void MainPanel::setupRow (Row& row,
                          const juce::String& rowName,
                          StripPreview::Kind previewKind,
                          const char* muteId,
                          const char* soloId,
                          juce::Colour accent)
{
    row.accent = accent;

    row.name.setText (rowName, juce::dontSendNotification);
    row.name.setFont (juce::Font (juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    row.name.setColour (juce::Label::textColourId, accent.withAlpha (0.85f));
    row.name.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (row.name);

    row.preview = std::make_unique<StripPreview> (proc, previewKind);
    addAndMakeVisible (*row.preview);

    auto setupToggle = [] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack);
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim);
        btn.setColour (juce::TextButton::textColourOnId, bridge::colors::textPrimary);
    };

    setupToggle (row.mute);
    setupToggle (row.solo);

    // Mute-on: red tint (universal danger indicator)
    row.mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF7A2828).withAlpha (0.65f));
    row.mute.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xFFFF8080));

    row.mute.setTooltip ("Mute Track");
    row.solo.setTooltip ("Solo Track");
    addAndMakeVisible (row.mute);
    addAndMakeVisible (row.solo);

    row.muteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, muteId, row.mute);
    row.soloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, soloId, row.solo);

    row.solo.onClick = [this] { syncSoloButtonColours(); };
}

void MainPanel::syncSoloButtonColours()
{
    using namespace bridge::colors;
    auto styleSolo = [] (juce::TextButton& b, bool isLit, juce::Colour accent)
    {
        b.setColour (juce::TextButton::buttonOnColourId, isLit ? accent.withAlpha(0.7f) : knobTrack);
        b.setColour (juce::TextButton::textColourOnId, isLit ? juce::Colours::white : textPrimary);
    };
    bool dS = proc.apvtsMain.getRawParameterValue ("mainSoloDrums")->load() > 0.5f;
    bool bS = proc.apvtsMain.getRawParameterValue ("mainSoloBass")->load() > 0.5f;
    bool pS = proc.apvtsMain.getRawParameterValue ("mainSoloPiano")->load() > 0.5f;
    bool gS = proc.apvtsMain.getRawParameterValue ("mainSoloGuitar")->load() > 0.5f;
    
    styleSolo (drumsRow.solo, dS, accentDrums);
    styleSolo (bassRow.solo, bS, accentBass);
    styleSolo (pianoRow.solo, pS, accentPiano);
    styleSolo (guitarRow.solo, gS, accentGuitar);
}

void MainPanel::valueTreePropertyChanged (juce::ValueTree& vt, const juce::Identifier& id)
{
    juce::ignoreUnused (vt, id);
}

void MainPanel::timerCallback()
{
    if (drumsRow.preview) drumsRow.preview->repaint();
    if (bassRow.preview) bassRow.preview->repaint();
    if (pianoRow.preview) pianoRow.preview->repaint();
    if (guitarRow.preview) guitarRow.preview->repaint();
}

void MainPanel::resized()
{
    using namespace bridge::instrumentLayout;

    const auto inner = getLocalBounds().reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 42);

    // ── Dropdowns (match Bass: Style, ROOT / SCALE / OCT, power) ──
    auto dropdownRow = inner.withHeight (42);
    bridge::panelLayout::layoutMelodicDropdownRow (dropdownRow, styleLabel, styleBox,
                                                   rootLabel, rootBox, scaleLabel, scaleBox,
                                                   octaveLabel, octaveBox, pagePower);

    // ── Mixer Lanes ──
    auto mixArea = shell.mainCard.reduced (10, 10);
    mixArea.removeFromTop (16); // step numbers (1..16) drawn in paint()

    constexpr int laneGap = 6;
    const int laneH = (mixArea.getHeight() - laneGap * 3) / 4;

    auto layoutLane = [&] (Row& row, juce::Rectangle<int> b) {
        row.bounds = b;

        // 4 px reserved for the accent stripe drawn in paint()
        b.removeFromLeft (4);
        auto leftCol = b.removeFromLeft (88);

        // Vertically centre name + buttons within the lane
        const int totalCtrlH = 13 + 4 + 20;
        const int ctrlY = leftCol.getY() + (leftCol.getHeight() - totalCtrlH) / 2;
        auto ctrl = leftCol.withY (ctrlY).withHeight (totalCtrlH);

        row.name.setBounds (ctrl.removeFromTop (13));
        ctrl.removeFromTop (4);

        auto btnsRow = ctrl.removeFromTop (20);
        row.mute.setBounds (btnsRow.removeFromLeft (28).withHeight (20));
        btnsRow.removeFromLeft (5);
        row.solo.setBounds (btnsRow.removeFromLeft (28).withHeight (20));

        b.removeFromLeft (6);
        row.preview->setBounds (b);
    };

    layoutLane (drumsRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (bassRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (pianoRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (guitarRow, mixArea.removeFromTop (laneH));

    bottomHalf.setBounds (shell.knobsCard.getUnion (shell.loopActionsCard));
}

void MainPanel::paint (juce::Graphics& g)
{
    using namespace bridge::instrumentLayout;
    g.fillAll (bridge::colors::background);

    const auto inner = getLocalBounds().reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 42);

    // "HARMONY" section label in the dropdown row area
    {
        auto harmonyLabelArea = inner.withHeight (12);
        g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Bold")));
        g.setColour (bridge::colors::textDim);
        g.drawText ("HARMONY", harmonyLabelArea.withTrimmedLeft (2),
                    juce::Justification::centredLeft, false);
    }

    // Main card
    {
        auto rf = shell.mainCard.toFloat();
        g.setColour (bridge::colors::cardSurface);
        g.fillRoundedRectangle (rf, (float) kCardRadius);
        g.setColour (bridge::colors::cardOutline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), (float) kCardRadius, 1.0f);
    }

    // Per-instrument accent stripes (3 px, left edge of each lane)
    auto drawAccentStripe = [&] (const Row& row)
    {
        if (row.bounds.isEmpty()) return;
        // Stripe sits at the left edge of the lane, inset slightly from top/bottom
        auto stripe = row.bounds.withWidth (3).toFloat().reduced (0.0f, 3.0f);
        g.setColour (row.accent.withAlpha (0.75f));
        g.fillRoundedRectangle (stripe, 1.5f);
    };
    drawAccentStripe (drumsRow);
    drawAccentStripe (bassRow);
    drawAccentStripe (pianoRow);
    drawAccentStripe (guitarRow);

    // Subtle lane separators
    {
        auto mixAreaP = shell.mainCard.reduced (10, 10);
        mixAreaP.removeFromTop (16);
        constexpr int laneGap = 6;
        const int laneH = (mixAreaP.getHeight() - laneGap * 3) / 4;
        float y = (float) mixAreaP.getY();
        for (int lane = 0; lane < 3; ++lane)
        {
            y += (float) laneH;
            g.setColour (bridge::colors::cardOutline.withAlpha (0.4f));
            g.drawHorizontalLine (juce::roundToInt (y + (float) laneGap * 0.5f),
                                  (float) mixAreaP.getX(), (float) mixAreaP.getRight());
            y += (float) laneGap;
        }
    }

    // Step numbers 1–16; beat positions (1, 5, 9, 13) rendered brighter
    {
        auto mixArea = shell.mainCard.reduced (10, 10);
        auto headerBox = mixArea.removeFromTop (16);
        headerBox.removeFromLeft (98); // align with preview strip start
        const float headerW = (float) headerBox.getWidth();
        const float cellW   = headerW / 16.0f;
        const float x0      = (float) headerBox.getX();

        g.setFont (juce::Font (juce::FontOptions().withHeight (9.5f).withStyle ("SemiBold")));
        for (int i = 0; i < 16; ++i)
        {
            const bool isBeat = (i % 4 == 0);
            g.setColour (isBeat ? bridge::colors::textSecondary : bridge::colors::textDim);
            const int x = juce::roundToInt (x0 + (float) i * cellW);
            g.drawText (juce::String (i + 1),
                        x, headerBox.getY(), juce::roundToInt (cellW), headerBox.getHeight(),
                        juce::Justification::centred, false);
        }
    }
}

// -------------------------------------------------------------

void MainPanel::StripPreview::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float cellW = b.getWidth() / 16.0f;
    const bool leaderOn = proc.apvtsMain.getRawParameterValue ("leaderTabOn") != nullptr
                              && proc.apvtsMain.getRawParameterValue ("leaderTabOn")->load() > 0.5f;

    g.setColour (juce::Colour (0xff18141f));
    g.fillAll();

    g.setColour (juce::Colour (0xff3a3548).withAlpha (0.2f));
    for (int i = 1; i < 16; ++i)
        g.drawVerticalLine (juce::roundToInt ((float) i * cellW), 0.0f, b.getHeight());

    if (kind == Kind::drums)
    {
        const float cellH = b.getHeight() / 8.0f;
        const auto& pat = proc.drumEngine.getPattern();

        for (int i = 1; i < 8; ++i)
            g.drawHorizontalLine (juce::roundToInt ((float) i * cellH), 0.0f, b.getWidth());

        for (int step = 0; step < 16; ++step)
        {
            const auto si = (size_t) step;
            for (int visualRow = 0; visualRow < 8; ++visualRow)
            {
                const int d = 7 - visualRow;
                if (pat[si][(size_t) d].active)
                {
                    const float vel = pat[si][(size_t) d].velocity / 127.0f;
                    g.setColour (bridge::colors::accentDrums.withAlpha (0.6f + vel * 0.4f));
                    g.fillRoundedRectangle ((float) step * cellW + 1.0f,
                                            (float) visualRow * cellH + 1.0f,
                                            cellW - 2.0f, cellH - 2.0f, 2.0f);
                }
            }
        }
        const int cur = proc.drumsCurrentVisualStep.load();
        if (leaderOn && cur >= 0 && cur < 16)
        {
            const float x = (float) cur * cellW;
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (x, 0.0f, cellW, b.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillRect (x, 0.0f, 2.0f, b.getHeight());
        }
    }
    else
    {
        auto drawMelodic = [&] (const auto& engine, juce::Colour col, int curStep)
        {
            const auto& grid = engine.getPatternForGrid();

            int minMidi = 60, maxMidi = 72;
            bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
            const int nRows = bridge::kMelodicOctaveRows;

            const float cellH = b.getHeight() / (float) nRows;

            for (int i = 1; i < nRows; ++i)
                g.drawHorizontalLine (juce::roundToInt ((float) i * cellH), 0.0f, b.getWidth());

            for (int i = 0; i < 16; ++i)
            {
                const auto gi = (size_t) i;
                if (grid[gi].active && grid[gi].velocity > 0)
                {
                    const int midi = juce::jlimit (minMidi, maxMidi, grid[gi].midiNote);
                    int displayRow = maxMidi - midi;
                    displayRow = juce::jlimit (0, nRows - 1, displayRow);

                    const float y = (float) displayRow * cellH;
                    g.setColour (col.withAlpha (0.6f + (grid[gi].velocity / 127.0f) * 0.4f));

                    auto block = juce::Rectangle<float> ((float) i * cellW, y, cellW, cellH)
                                       .reduced (1.0f, cellH * 0.2f);
                    g.fillRoundedRectangle (block, 3.0f);
                }
            }
            if (leaderOn && curStep >= 0 && curStep < 16)
            {
                const float x = (float) curStep * cellW;
                g.setColour (juce::Colours::white.withAlpha (0.2f));
                g.fillRect (x, 0.0f, cellW, b.getHeight());
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (x, 0.0f, 2.0f, b.getHeight());
            }
        };

        if (kind == Kind::bass)
            drawMelodic (proc.bassEngine, bridge::colors::accentBass, proc.bassCurrentVisualStep.load());
        else if (kind == Kind::piano)
            drawMelodic (proc.pianoEngine, bridge::colors::accentPiano, proc.pianoCurrentVisualStep.load());
        else if (kind == Kind::guitar)
            drawMelodic (proc.guitarEngine, bridge::colors::accentGuitar, proc.guitarCurrentVisualStep.load());
    }
}
