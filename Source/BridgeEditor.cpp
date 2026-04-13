#include "BridgeEditor.h"
#include "BridgeInstrumentStyles.h"
#include "BridgeSettingsDialog.h"
#include "drums/DrumsLookAndFeel.h"
#include "bass/BassLookAndFeel.h"
#include "piano/PianoLookAndFeel.h"
#include "guitar/GuitarLookAndFeel.h"

namespace
{
static bool paramOn (juce::AudioProcessorValueTreeState& ap, const char* id)
{
    if (auto* v = ap.getRawParameterValue (id))
        return v->load() > 0.5f;
    return true;
}

/** Shared outline + base fill colours for header transport shape buttons. */
static void setupTransportBtn (juce::ShapeButton& b)
{
    b.setColours (juce::Colour (0xffeae2d5), juce::Colour (0xfff0ebe4), juce::Colour (0xffd8d0c8));
    b.setOutline (juce::Colour (0xff9e99a8), 1.0f);
}

// Instrument accent colours — pulled from each panel's M3 palette.
inline juce::Colour accentForTab (int tab)
{
    switch (tab)
    {
        case 0: return juce::Colour (0xffd4a84b); // Leader gold
        case 1: return juce::Colour (0xffe07a5a); // Drums coral
        case 2: return juce::Colour (0xff5cb8a8); // Bass teal
        case 3: return juce::Colour (0xffb88cff); // Piano violet
        case 4: return juce::Colour (0xff6eb3ff); // Guitar sky blue
        default: return juce::Colour (0xffeae2d5);
    }
}
}

BridgeEditor::BridgeEditor (BridgeProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      mainPanel (p),
      drumsPanel (p),
      bassPanel (p),
      pianoPanel (p),
      guitarPanel (p)
{
    using namespace bridge::instrumentLayout;
    setSize (kWindowW, kWindowH);
    setResizable (false, false);

    // ── Logo ───────────────────────────────────────────────────────────────
    logoLabel.setText ("BRIDGE", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (juce::FontOptions().withHeight (18.0f).withStyle ("Semibold")));
    logoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffeae2d5));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    // ── BPM display ────────────────────────────────────────────────────────
    bpmValueLabel.setText ("120", juce::dontSendNotification);
    bpmValueLabel.setFont (juce::Font (juce::FontOptions().withHeight (16.0f).withStyle ("Semibold")));
    bpmValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xffeae2d5));
    bpmValueLabel.setJustificationType (juce::Justification::centredRight);
    bpmValueLabel.setEditable (true, false, false);
    bpmValueLabel.onTextChange = [this]
    {
        if (proc.apvtsMain.getRawParameterValue ("internalBpm") != nullptr)
        {
            auto newVal = bpmValueLabel.getText().getFloatValue();
            newVal = juce::jlimit (40.0f, 240.0f, newVal);
            proc.apvtsMain.getParameter ("internalBpm")->setValueNotifyingHost (
                proc.apvtsMain.getParameter ("internalBpm")->getNormalisableRange().convertTo0to1 (newVal));
        }
    };
    addAndMakeVisible (bpmValueLabel);

    bpmUnitLabel.setText ("BPM", juce::dontSendNotification);
    bpmUnitLabel.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Semibold")));
    bpmUnitLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9e99a8));
    bpmUnitLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmUnitLabel);

    // ── Settings (Gear) Button ─────────────────────────────────────────────
    juce::Path gearPath;
    gearPath.addStar (juce::Point<float> (0.0f, 0.0f), 8, 4.0f, 6.0f, 0.0f);
    gearPath.addEllipse (-2.0f, -2.0f, 4.0f, 4.0f); // Make it a gear
    gearPath.setUsingNonZeroWinding (false);
    gearButton.setShape (gearPath, false, true, false);
    gearButton.setOnColours (juce::Colour (0xffeae2d5), juce::Colour (0xffeae2d5), juce::Colour (0xffeae2d5));
    gearButton.setOutline (juce::Colour (0xff9e99a8), 1.5f);
    gearButton.onClick = [this]
    {
        auto* content = new BridgeSettingsDialog();
        content->setSize (520, 440);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (content);
        o.dialogTitle                   = "Settings";
        o.dialogBackgroundColour        = juce::Colour (0xff14121a);
        o.useNativeTitleBar             = true;
        o.resizable                     = false;
        o.useBottomRightCornerResizer   = false;
        o.launchAsync();
    };
    addAndMakeVisible (gearButton);

    
    // ── Play / Stop (square buttons, icon scaled to fit) ───────────────────
    juce::Path playPath;
    playPath.addTriangle (2.0f, 1.0f, 12.0f, 8.0f, 2.0f, 15.0f);
    playButton.setShape (playPath, true, true, false);
    playButton.setClickingTogglesState (true);
    playButton.shouldUseOnColours (true);
    playButton.setOnColours (juce::Colour (0xff6ee7a0), juce::Colour (0xff8ef0b0), juce::Colour (0xff5bc080));
    setupTransportBtn (playButton);
    transportAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "transportPlaying", playButton);
    addAndMakeVisible (playButton);

    juce::Path stopPath;
    stopPath.addRectangle (3.0f, 3.0f, 10.0f, 10.0f);
    stopButton.setShape (stopPath, true, true, false);
    stopButton.setClickingTogglesState (false);
    setupTransportBtn (stopButton);
    stopButton.onClick = [this]
    {
        if (auto* pr = proc.apvtsMain.getParameter ("transportPlaying"))
            pr->setValueNotifyingHost (0.0f);
    };
    addAndMakeVisible (stopButton);

    // ── Host Sync ──────────────────────────────────────────────────────────
    hostSyncButton.setClickingTogglesState (true);
    hostSyncButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23202b));
    hostSyncButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a4a38));
    hostSyncButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9e99a8));
    hostSyncButton.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff6ee7a0));
    hostSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (proc.apvtsMain, "hostSync", hostSyncButton);
    addAndMakeVisible (hostSyncButton);

    // ── Tab chips ──────────────────────────────────────────────────────────
    juce::TextButton* tabs[] = { &tabMain, &tabDrums, &tabBass, &tabPiano, &tabGuitar };
    for (auto* t : tabs)
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (42);
        t->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        t->setConnectedEdges (0);
        addAndMakeVisible (*t);
    }

    tabMain.onClick   = [this] { proc.activeTab.store (0); showTab (0); };
    tabDrums.onClick = [this] { proc.activeTab.store (1); showTab (1); };
    tabBass.onClick = [this] { proc.activeTab.store (2); showTab (2); };
    tabPiano.onClick = [this] { proc.activeTab.store (3); showTab (3); };
    tabGuitar.onClick   = [this] { proc.activeTab.store (4); showTab (4); };

    mainPanel.onOpenTab = [this](int tabIndex)
    {
        if (tabIndex == 1) { tabDrums.setToggleState (true, juce::sendNotification); }
        else if (tabIndex == 2) { tabBass.setToggleState (true, juce::sendNotification); }
        else if (tabIndex == 3) { tabPiano.setToggleState (true, juce::sendNotification); }
        else if (tabIndex == 4) { tabGuitar.setToggleState (true, juce::sendNotification); }
    };

    addChildComponent (mainPanel);
    addChildComponent (drumsPanel);
    addChildComponent (bassPanel);
    addChildComponent (pianoPanel);
    addChildComponent (guitarPanel);

    proc.apvtsMain.state.addListener (this);

    const int t = juce::jlimit (0, 4, proc.activeTab.load());
    tabMain.setToggleState   (t == 0, juce::dontSendNotification);
    tabDrums.setToggleState (t == 1, juce::dontSendNotification);
    tabBass.setToggleState (t == 2, juce::dontSendNotification);
    tabPiano.setToggleState (t == 3, juce::dontSendNotification);
    tabGuitar.setToggleState   (t == 4, juce::dontSendNotification);
    showTab (t);

    setWantsKeyboardFocus (true);

    startTimerHz (4);
}

BridgeEditor::~BridgeEditor()
{
    stopTimer();
    proc.apvtsMain.state.removeListener (this);
}

bool BridgeEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (auto* pr = proc.apvtsMain.getParameter ("transportPlaying"))
        {
            pr->setValueNotifyingHost (pr->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        return true;
    }
    return false;
}

void BridgeEditor::timerCallback()
{
    updateBpmDisplay();
}

void BridgeEditor::updateBpmDisplay()
{
    if (bpmValueLabel.isBeingEdited())
        return;

    const bool hostOn = paramOn (proc.apvtsMain, "hostSync");
    float bpm = 120.0f;
    if (hostOn)
    {
        // Mirror the value we last saw from the host's transport info.
        bpm = (float) proc.currentHostBpm.load();
    }
    else if (auto* v = proc.apvtsMain.getRawParameterValue ("internalBpm"))
    {
        bpm = v->load();
    }

    bpmValueLabel.setText (juce::String ((int) std::round (bpm)), juce::dontSendNotification);
}

void BridgeEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    updateTabStripFromParams();
}

void BridgeEditor::updateTabStripFromParams()
{
    const int active = juce::jlimit (0, 4, proc.activeTab.load());
    const juce::Colour chipBg    (0xff1b1823);
    const juce::Colour chipTxDim (0xff9e99a8);
    const juce::Colour chipTxOn  (0xffeae2d5);

    juce::TextButton* tabs[] = { &tabMain, &tabDrums, &tabBass, &tabPiano, &tabGuitar };
    const char* ids[] = { "leaderTabOn", "drumsOn", "bassOn", "pianoOn", "guitarOn" };

    for (int i = 0; i < 5; ++i)
    {
        auto& t = *tabs[i];
        const bool on  = paramOn (proc.apvtsMain, ids[i]);
        const bool sel = (active == i);
        const auto accent = accentForTab (i);

        if (sel)
        {
            t.setColour (juce::TextButton::buttonColourId, chipBg);
            t.setColour (juce::TextButton::buttonOnColourId, chipBg);
            t.setColour (juce::TextButton::textColourOffId, accent);
            t.setColour (juce::TextButton::textColourOnId,  accent);
        }
        else
        {
            t.setColour (juce::TextButton::buttonColourId, chipBg);
            t.setColour (juce::TextButton::buttonOnColourId, chipBg);
            t.setColour (juce::TextButton::textColourOffId, chipTxDim);
            t.setColour (juce::TextButton::textColourOnId,  chipTxDim);
        }
        t.setAlpha (on ? 1.0f : 0.42f);
    }
}

void BridgeEditor::paint (juce::Graphics& g)
{
    using namespace bridge::instrumentLayout;
    g.fillAll (juce::Colour (0xff14121a));

    // Divider under the header
    g.setColour (juce::Colour (0xff3a3548).withAlpha (0.55f));
    g.drawHorizontalLine (kHeaderH - 1, 0.0f, (float) getWidth());

    // Small accent dot next to the active tab
    const int active = juce::jlimit (0, 4, proc.activeTab.load());
    auto accent = accentForTab (active);

    juce::TextButton* tabs[] = { &tabMain, &tabDrums, &tabBass, &tabPiano, &tabGuitar };
    for (int i = 0; i < 5; ++i)
    {
        auto b = tabs[i]->getBounds().toFloat();
        const float dotR = 3.5f;
        const float dotX = b.getX() + 12.0f;
        const float dotY = b.getCentreY();
        const bool sel = (i == active);
        const auto col = sel ? accentForTab (i) : juce::Colour (0xff6a6578);
        g.setColour (col);
        g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2, dotR * 2);
    }

    juce::ignoreUnused (accent);
}

void BridgeEditor::resized()
{
    using namespace bridge::instrumentLayout;
    auto r = getLocalBounds();

    // ── Header bar ─────────────────────────────────────────────────────────
    auto header = r.removeFromTop (kHeaderH).reduced (14, 14);
    const int tabH = kHeaderControlH;
    const int tabW = 60;
    const int tabGap = 6;
    const int tabsTotalW = tabW * 5 + tabGap * 4;
    const int sq = tabH;

    auto h = header;

    gearButton.setBounds (h.removeFromRight (sq).withSizeKeepingCentre (sq, sq));
    h.removeFromRight (8);

    auto tabArea = h.removeFromRight (tabsTotalW);
    juce::TextButton* tabs[] = { &tabMain, &tabDrums, &tabBass, &tabPiano, &tabGuitar };
    for (int i = 0; i < 5; ++i)
    {
        if (i > 0)
            tabArea.removeFromLeft (tabGap);
        tabs[i]->setBounds (tabArea.removeFromLeft (tabW).withSizeKeepingCentre (tabW, tabH));
    }

    logoLabel.setBounds (h.removeFromLeft (96));
    h.removeFromLeft (8);

    auto transport = h.removeFromLeft (300);
    const int ty = transport.getY() + (transport.getHeight() - tabH) / 2;
    {
        auto tb = transport;
        bpmValueLabel.setBounds (tb.removeFromLeft (40).withY (ty).withHeight (tabH));
        tb.removeFromLeft (4);
        bpmUnitLabel.setBounds (tb.removeFromLeft (28).withY (ty).withHeight (tabH));
        tb.removeFromLeft (10);

        playButton.setBounds (tb.removeFromLeft (sq + 4).withSizeKeepingCentre (sq, sq));
        tb.removeFromLeft (6);
        stopButton.setBounds (tb.removeFromLeft (sq + 4).withSizeKeepingCentre (sq, sq));
        tb.removeFromLeft (10);

        hostSyncButton.setBounds (tb.removeFromLeft (96).withY (ty).withHeight (tabH));
    }

    // ── Panel area (everything below the header) ───────────────────────────
    mainPanel.setBounds   (r);
    drumsPanel.setBounds (r);
    bassPanel.setBounds (r);
    pianoPanel.setBounds (r);
    guitarPanel.setBounds   (r);

    updateTabStripFromParams();
}

void BridgeEditor::showTab (int index)
{
    mainPanel.setVisible   (index == 0);
    drumsPanel.setVisible (index == 1);
    bassPanel.setVisible (index == 2);
    pianoPanel.setVisible (index == 3);
    guitarPanel.setVisible   (index == 4);

    if (index == 0)        mainPanel.toFront (true);
    else if (index == 1)   drumsPanel.toFront (true);
    else if (index == 2)   bassPanel.toFront (true);
    else if (index == 3)   pianoPanel.toFront (true);
    else                   guitarPanel.toFront (true);

    updateTabStripFromParams();
    repaint();
}

void BridgeEditor::notifyDrumsPatternChanged()
{
    drumsPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyBassPatternChanged()
{
    bassPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyPianoPatternChanged()
{
    pianoPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyGuitarPatternChanged()
{
    guitarPanel.triggerAsyncUpdate();
}
