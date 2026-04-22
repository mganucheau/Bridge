#include "MainPanel.h"
#include "BridgeAppleHIG.h"
#include "MelodicGridLayout.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStripStyle.h"
#include "BridgeInstrumentStyles.h"
#include "BridgeScaleHighlight.h"

MainPanel::MainPanel (BridgeProcessor& p)
    : proc (p),
      instrumentStrip (InstrumentControlBar::makeLeaderConfig (p)),
      bottomHalf (p.apvtsMain, p.apvtsMain, laf, bridge::colors::accentLeader(),
                  [&p] { p.triggerDrumsGenerate(); p.triggerBassGenerate(); p.triggerPianoGenerate(); p.triggerGuitarGenerate(); },
                  [&p] (bool active) { 
                      p.drumEngine.setFillHoldActive(active); 
                      p.bassEngine.setFillHoldActive(active); 
                      p.pianoEngine.setFillHoldActive(active); 
                      p.guitarEngine.setFillHoldActive(active); 
                  })
{
    setLookAndFeel (&laf);

    proc.apvtsMain.state.addListener (this);

    addAndMakeVisible (instrumentStrip);

    setupRow (drumsRow,  "DRUMS",  StripPreview::Kind::drums,  "mainMuteDrums",  "mainSoloDrums");
    setupRow (bassRow,   "BASS",   StripPreview::Kind::bass,   "mainMuteBass",   "mainSoloBass");
    setupRow (pianoRow,  "PIANO",  StripPreview::Kind::piano,  "mainMutePiano",  "mainSoloPiano");
    setupRow (guitarRow, "GUITAR", StripPreview::Kind::guitar, "mainMuteGuitar", "mainSoloGuitar");

    instrumentStrip.getMuteButton().setTooltip ("Mute Leader (arrangement off)");
    instrumentStrip.getSoloButton().setTooltip ("Solo all tracks");

    instrumentStrip.getMuteButton().onClick = [this]
    {
        const bool muted = instrumentStrip.getMuteButton().getToggleState();
        if (auto* par = proc.apvtsMain.getParameter ("leaderTabOn"))
            par->setValueNotifyingHost (muted ? 0.0f : 1.0f);
    };

    instrumentStrip.getSoloButton().onClick = [this]
    {
        const bool v = instrumentStrip.getSoloButton().getToggleState();
        const float norm = v ? 1.0f : 0.0f;
        for (const char* id : { "mainSoloDrums", "mainSoloBass", "mainSoloPiano", "mainSoloGuitar" })
            if (auto* par = proc.apvtsMain.getParameter (id))
                par->setValueNotifyingHost (norm);
        syncSoloButtonColours();
        syncPageSoloToggle();
    };

    addAndMakeVisible (bottomHalf);
    proc.apvtsMain.addParameterListener ("leaderTabOn", this);
    for (const char* id : { "mainSoloDrums", "mainSoloBass", "mainSoloPiano", "mainSoloGuitar" })
        proc.apvtsMain.addParameterListener (id, this);
    for (const char* id : { "presence", "density", "swing", "humanize", "pocket", "velocity", "fillRate",
                            "complexity", "ghostAmount", "leaderStyle", "perform", "scale", "rootNote" })
        proc.apvtsMain.addParameterListener (id, this);

    startTimerHz (30);
    syncSoloButtonColours();
    syncPageSoloToggle();
    applyLeaderPageState();
}

MainPanel::~MainPanel()
{
    proc.apvtsMain.removeParameterListener ("leaderTabOn", this);
    for (const char* id : { "mainSoloDrums", "mainSoloBass", "mainSoloPiano", "mainSoloGuitar" })
        proc.apvtsMain.removeParameterListener (id, this);
    for (const char* id : { "presence", "density", "swing", "humanize", "pocket", "velocity", "fillRate",
                            "complexity", "ghostAmount", "leaderStyle", "perform", "scale", "rootNote" })
        proc.apvtsMain.removeParameterListener (id, this);
    proc.apvtsMain.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void MainPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "leaderTabOn")
        applyLeaderPageState();
    else if (parameterID == "mainSoloDrums" || parameterID == "mainSoloBass"
             || parameterID == "mainSoloPiano" || parameterID == "mainSoloGuitar")
    {
        syncPageSoloToggle();
        syncSoloButtonColours();
    }
    else if (parameterID == "presence" || parameterID == "density" || parameterID == "swing"
             || parameterID == "humanize" || parameterID == "pocket" || parameterID == "velocity"
             || parameterID == "fillRate" || parameterID == "complexity" || parameterID == "ghostAmount"
             || parameterID == "leaderStyle" || parameterID == "perform" || parameterID == "scale"
             || parameterID == "rootNote")
    {
        proc.rebuildDrumsGridPreview();
        proc.rebuildBassGridPreview();
        proc.rebuildPianoGridPreview();
        proc.rebuildGuitarGridPreview();
        if (drumsRow.preview != nullptr) drumsRow.preview->repaint();
        if (bassRow.preview != nullptr) bassRow.preview->repaint();
        if (pianoRow.preview != nullptr) pianoRow.preview->repaint();
        if (guitarRow.preview != nullptr) guitarRow.preview->repaint();
    }
}

void MainPanel::syncPageSoloToggle()
{
    const bool d = proc.apvtsMain.getRawParameterValue ("mainSoloDrums")->load() > 0.5f;
    const bool b = proc.apvtsMain.getRawParameterValue ("mainSoloBass")->load() > 0.5f;
    const bool p = proc.apvtsMain.getRawParameterValue ("mainSoloPiano")->load() > 0.5f;
    const bool g = proc.apvtsMain.getRawParameterValue ("mainSoloGuitar")->load() > 0.5f;
    instrumentStrip.getSoloButton().setToggleState (d && b && p && g, juce::dontSendNotification);
}

void MainPanel::applyLeaderPageState()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("leaderTabOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("leaderTabOn")->load() > 0.5f;

    bottomHalf.setEnabled (on);
    instrumentStrip.setTrackPowered (on);

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
                          const char* soloId)
{
    row.name.setText (rowName, juce::dontSendNotification);
    row.name.setFont (bridge::hig::uiFont (13.0f, "Bold"));
    row.name.setColour (juce::Label::textColourId, bridge::colors::textPrimary());
    row.name.setJustificationType (juce::Justification::centredTop);
    addAndMakeVisible (row.name);

    row.preview = std::make_unique<StripPreview> (proc, previewKind);
    addAndMakeVisible (*row.preview);

    row.name.setFont (bridge::hig::uiFont (19.0f, "Bold"));

    auto setupMute = [] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (0);
        bridge::instrumentStripStyle::tagStripMute (btn);
        btn.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
        btn.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
        btn.setColour (juce::TextButton::textColourOffId, bridge::instrumentStripStyle::msOffText());
        btn.setColour (juce::TextButton::textColourOnId, bridge::instrumentStripStyle::muteOn());
    };
    auto setupSoloLane = [] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (0);
        bridge::instrumentStripStyle::tagStripSolo (btn);
        btn.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
        btn.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
        btn.setColour (juce::TextButton::textColourOffId, bridge::instrumentStripStyle::msOffText());
        btn.setColour (juce::TextButton::textColourOnId, bridge::instrumentStripStyle::soloOn());
    };

    setupMute (row.mute);
    setupSoloLane (row.solo);
    row.mute.setTooltip ("Mute Track");
    row.solo.setTooltip ("Solo Track");
    addAndMakeVisible (row.mute);
    addAndMakeVisible (row.solo);

    row.muteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, muteId, row.mute);
    row.soloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvtsMain, soloId, row.solo);

    row.mute.onClick = [this] { syncSoloButtonColours(); };
    row.solo.onClick = [this] { syncSoloButtonColours(); syncPageSoloToggle(); };
}

void MainPanel::syncSoloButtonColours()
{
    auto styleSolo = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
        b.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
        b.setColour (juce::TextButton::textColourOffId, bridge::instrumentStripStyle::msOffText());
        b.setColour (juce::TextButton::textColourOnId, bridge::instrumentStripStyle::soloOn());
    };
    styleSolo (drumsRow.solo);
    styleSolo (bassRow.solo);
    styleSolo (pianoRow.solo);
    styleSolo (guitarRow.solo);
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

    auto bounds = getLocalBounds();
    auto stripRow = bounds.removeFromTop (kDropdownH);
    instrumentStrip.setBounds (stripRow);
    auto inner = bounds.reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 0);

    // ── Mixer Lanes ──
    auto mixArea = shell.mainCard.reduced (10, 10);
    mixArea.removeFromTop (16); // step numbers (1..16) drawn in paint()

    constexpr int laneGap = 4;
    const int laneH = (mixArea.getHeight() - laneGap * 3) / 4;

    auto layoutLane = [&] (Row& row, juce::Rectangle<int> b) {
        row.bounds = b;

        constexpr int kGutterW = 64;
        constexpr int kBtn = 22;
        constexpr int kBtnGap = 4;
        auto gutter = b.removeFromLeft (kGutterW);
        row.name.setBounds (gutter.removeFromTop (14));
        gutter.removeFromTop (2);
        auto btnStrip = gutter.removeFromTop (kBtn);
        const int bx0 = (kGutterW - (kBtn * 2 + kBtnGap)) / 2;
        row.mute.setBounds (btnStrip.getX() + bx0, btnStrip.getY(), kBtn, kBtn);
        row.solo.setBounds (btnStrip.getX() + bx0 + kBtn + kBtnGap, btnStrip.getY(), kBtn, kBtn);

        b.removeFromLeft (8);
        row.preview->setBounds (b);
    };

    layoutLane (drumsRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (bassRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (pianoRow, mixArea.removeFromTop (laneH));
    mixArea.removeFromTop (laneGap);
    layoutLane (guitarRow, mixArea.removeFromTop (laneH));

    bottomHalf.setBounds (shell.bottomStrip);
}

void MainPanel::paint (juce::Graphics& g)
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

    // Subtle separators between leader lanes (in the gap between strips)
    {
        auto mixAreaP = shell.mainCard.reduced (10, 10);
        mixAreaP.removeFromTop (16);
        constexpr int laneGap = 4;
        const int laneH = (mixAreaP.getHeight() - laneGap * 3) / 4;
        float y = (float) mixAreaP.getY();
        for (int lane = 0; lane < 3; ++lane)
        {
            y += (float) laneH;
            g.setColour (bridge::colors::cardOutline().withAlpha (0.4f));
            g.drawHorizontalLine (juce::roundToInt (y + (float) laneGap * 0.5f),
                                  (float) mixAreaP.getX(), (float) mixAreaP.getRight());
            y += (float) laneGap;
        }
    }

    // Draw 1..16 grid headers
    g.setColour (bridge::colors::textDim());
    g.setFont (bridge::hig::uiFont (11.0f));
    auto mixArea = shell.mainCard.reduced (10, 10);
    auto headerBox = mixArea.removeFromTop (16);
    // 64 (lane gutter) + 8 gap + 64 (strip inside preview) — align with melodic pages
    headerBox.removeFromLeft (136);
    const float headerW = (float) headerBox.getWidth();
    const float cellW = headerW / 16.0f;
    const float x0 = (float) headerBox.getX();
    for (int i = 0; i < 16; ++i)
    {
        const int x = juce::roundToInt (x0 + (float) i * cellW);
        g.drawText (juce::String (i + 1),
                    x, headerBox.getY(), juce::roundToInt (cellW), headerBox.getHeight(),
                    juce::Justification::centred, false);
    }
}

// -------------------------------------------------------------

void MainPanel::StripPreview::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    constexpr float kSide = 64.0f;
    auto gridRect = full;
    gridRect.removeFromLeft (kSide);

    const float cellW = gridRect.getWidth() / 16.0f;
    const bool leaderOn = proc.apvtsMain.getRawParameterValue ("leaderTabOn") != nullptr
                              && proc.apvtsMain.getRawParameterValue ("leaderTabOn")->load() > 0.5f;

    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillAll();

    // Left strip: drum rack (rows) or mini keyboard (melodic)
    {
        auto strip = full.withWidth (kSide);
        g.setColour (bridge::hig::tertiaryGroupedBackground);
        g.fillRect (strip);
        g.setColour (bridge::hig::separatorOpaque.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (strip.getRight()), strip.getY(), strip.getBottom());

        if (kind == Kind::drums)
        {
            const float sh = strip.getHeight() / (float) NUM_DRUMS;
            for (int i = 1; i < NUM_DRUMS; ++i)
                g.drawHorizontalLine (juce::roundToInt (strip.getY() + (float) i * sh),
                                      strip.getX(), strip.getRight());
            g.setFont (bridge::hig::uiFont (9.0f, "Semibold"));
            for (int vr = 0; vr < NUM_DRUMS; ++vr)
            {
                const int d = NUM_DRUMS - 1 - vr;
                auto row = strip.withHeight (sh).withY (strip.getY() + (float) vr * sh);
                g.setColour (bridge::colors::textDim());
                g.drawText (juce::String (DRUM_NAMES[d]).substring (0, 2),
                            row.reduced (2.0f, 0.0f), juce::Justification::centred, true);
            }
        }
        else
        {
            const int scaleIdx = proc.apvtsMain.getRawParameterValue ("scale") != nullptr
                                     ? (int) proc.apvtsMain.getRawParameterValue ("scale")->load()
                                     : 0;
            const int rootPc = proc.apvtsMain.getRawParameterValue ("rootNote") != nullptr
                                   ? (int) proc.apvtsMain.getRawParameterValue ("rootNote")->load()
                                   : 0;
            auto paintMiniKeys = [&] (auto& engine)
            {
                int low = 60, high = 72;
                bridge::setOneOctaveMelodicRange (engine, low, high);
                const int nRows = bridge::kMelodicOctaveRows;
                const float rowH = strip.getHeight() / (float) nRows;
                for (int r = 0; r < nRows; ++r)
                {
                    const int midi = high - r;
                    auto row = strip.withHeight (rowH).withY (strip.getY() + (float) r * rowH);
                    const bool bk = (midi % 12 == 1 || midi % 12 == 3 || midi % 12 == 6
                                    || midi % 12 == 8 || midi % 12 == 10);
                    const bool inScale = bridge::pitchClassInPresetScale (scaleIdx, rootPc, midi % 12);
                    juce::Colour fill = bk ? bridge::hig::systemBackground : bridge::hig::secondaryLabel;
                    if (! inScale)
                        fill = fill.interpolatedWith (bridge::hig::tertiaryGroupedBackground, 0.55f);
                    g.setColour (fill);
                    g.fillRect (row.reduced (0.0f, 0.5f));
                }
                g.setColour (bridge::hig::separatorOpaque.withAlpha (0.35f));
                for (int i = 1; i < nRows; ++i)
                    g.drawHorizontalLine (juce::roundToInt (strip.getY() + (float) i * rowH),
                                          strip.getX(), strip.getRight());
            };
            if (kind == Kind::bass)   paintMiniKeys (proc.bassEngine);
            else if (kind == Kind::piano) paintMiniKeys (proc.pianoEngine);
            else                          paintMiniKeys (proc.guitarEngine);
        }
    }

    auto gx0 = gridRect.getX();
    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.35f));
    for (int i = 1; i < 16; ++i)
        g.drawVerticalLine (juce::roundToInt (gx0 + (float) i * cellW), gridRect.getY(), gridRect.getBottom());

    if (kind == Kind::drums)
    {
        const float cellH = gridRect.getHeight() / (float) NUM_DRUMS;
        const auto& pat = proc.getPatternForGrid();

        for (int i = 1; i < NUM_DRUMS; ++i)
            g.drawHorizontalLine (juce::roundToInt (gridRect.getY() + (float) i * cellH),
                                  gx0, gridRect.getRight());

        for (int step = 0; step < 16; ++step)
        {
            const auto si = (size_t) step;
            for (int visualRow = 0; visualRow < NUM_DRUMS; ++visualRow)
            {
                const int d = NUM_DRUMS - 1 - visualRow;
                if (pat[si][(size_t) d].active)
                {
                    const float vel = pat[si][(size_t) d].velocity / 127.0f;
                    g.setColour (bridge::colors::accentDrums().withAlpha (0.6f + vel * 0.4f));
                    g.fillRect (gx0 + (float) step * cellW + 1.0f,
                                gridRect.getY() + (float) visualRow * cellH + 1.0f,
                                cellW - 2.0f, cellH - 2.0f);
                }
            }
        }
        const int cur = proc.drumsCurrentVisualStep.load();
        if (leaderOn && cur >= 0 && cur < 16)
        {
            const float x = gx0 + (float) cur * cellW;
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (x, gridRect.getY(), cellW, gridRect.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillRect (x, gridRect.getY(), 2.0f, gridRect.getHeight());
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

            const float cellH = gridRect.getHeight() / (float) nRows;

            for (int i = 1; i < nRows; ++i)
                g.drawHorizontalLine (juce::roundToInt (gridRect.getY() + (float) i * cellH),
                                      gx0, gridRect.getRight());

            for (int i = 0; i < 16; ++i)
            {
                const auto gi = (size_t) i;
                if (grid[gi].active && grid[gi].velocity > 0)
                {
                    const int midi = juce::jlimit (minMidi, maxMidi, grid[gi].midiNote);
                    int displayRow = maxMidi - midi;
                    displayRow = juce::jlimit (0, nRows - 1, displayRow);

                    const float y = gridRect.getY() + (float) displayRow * cellH;
                    g.setColour (col.withAlpha (0.6f + (grid[gi].velocity / 127.0f) * 0.4f));

                    auto block = juce::Rectangle<float> (gx0 + (float) i * cellW, y, cellW, cellH)
                                       .reduced (1.0f, cellH * 0.2f);
                    g.fillRect (block);
                }
            }
            if (leaderOn && curStep >= 0 && curStep < 16)
            {
                const float x = gx0 + (float) curStep * cellW;
                g.setColour (juce::Colours::white.withAlpha (0.2f));
                g.fillRect (x, gridRect.getY(), cellW, gridRect.getHeight());
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (x, gridRect.getY(), 2.0f, gridRect.getHeight());
            }
        };

        if (kind == Kind::bass)
            drawMelodic (proc.bassEngine, bridge::colors::accentBass(), proc.bassCurrentVisualStep.load());
        else if (kind == Kind::piano)
            drawMelodic (proc.pianoEngine, bridge::colors::accentPiano(), proc.pianoCurrentVisualStep.load());
        else if (kind == Kind::guitar)
            drawMelodic (proc.guitarEngine, bridge::colors::accentGuitar(), proc.guitarCurrentVisualStep.load());
    }
}
