#include "BridgeEditor.h"
#include "BridgeInstrumentStyles.h"
#include "animal/AnimalLookAndFeel.h"
#include "bootsy/BootsyLookAndFeel.h"
#include "stevie/StevieLookAndFeel.h"
#include "paul/PaulLookAndFeel.h"

namespace
{
static bool paramOn (juce::AudioProcessorValueTreeState& ap, const char* id)
{
    if (auto* v = ap.getRawParameterValue (id))
        return v->load() > 0.5f;
    return true;
}

// Instrument accent colours — pulled from each panel's M3 palette.
inline juce::Colour accentForTab (int tab)
{
    switch (tab)
    {
        case 0: return juce::Colour (0xffd4a84b); // Leader gold
        case 1: return juce::Colour (0xffe07a5a); // Animal coral
        case 2: return juce::Colour (0xff5cb8a8); // Bootsy teal
        case 3: return juce::Colour (0xffb88cff); // Stevie violet
        case 4: return juce::Colour (0xff6eb3ff); // Paul sky blue
        default: return juce::Colour (0xffeae2d5);
    }
}
}

BridgeEditor::BridgeEditor (BridgeProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      mainPanel (p),
      animalPanel (p),
      bootsyPanel (p),
      steviePanel (p),
      paulPanel (p)
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
    bpmValueLabel.setFont (juce::Font (juce::FontOptions().withHeight (20.0f).withStyle ("Semibold")));
    bpmValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xffeae2d5));
    bpmValueLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmValueLabel);

    bpmUnitLabel.setText ("BPM", juce::dontSendNotification);
    bpmUnitLabel.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Semibold")));
    bpmUnitLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9e99a8));
    bpmUnitLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmUnitLabel);

    // ── Play / Stop ────────────────────────────────────────────────────────
    auto setupTransportBtn = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23202b));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a3548));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeae2d5));
        b.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff6ee7a0));
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    };

    playButton.setClickingTogglesState (true);
    setupTransportBtn (playButton);
    transportAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                         (proc.apvtsMain, "transportPlaying", playButton);
    addAndMakeVisible (playButton);

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
    juce::TextButton* tabs[] = { &tabMain, &tabAnimal, &tabBootsy, &tabStevie, &tabPaul };
    for (auto* t : tabs)
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (42);
        t->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        t->setConnectedEdges (0);
        addAndMakeVisible (*t);
    }

    tabMain.onClick   = [this] { proc.activeTab.store (0); showTab (0); };
    tabAnimal.onClick = [this] { proc.activeTab.store (1); showTab (1); };
    tabBootsy.onClick = [this] { proc.activeTab.store (2); showTab (2); };
    tabStevie.onClick = [this] { proc.activeTab.store (3); showTab (3); };
    tabPaul.onClick   = [this] { proc.activeTab.store (4); showTab (4); };

    addChildComponent (mainPanel);
    addChildComponent (animalPanel);
    addChildComponent (bootsyPanel);
    addChildComponent (steviePanel);
    addChildComponent (paulPanel);

    proc.apvtsMain.state.addListener (this);

    const int t = juce::jlimit (0, 4, proc.activeTab.load());
    tabMain.setToggleState   (t == 0, juce::dontSendNotification);
    tabAnimal.setToggleState (t == 1, juce::dontSendNotification);
    tabBootsy.setToggleState (t == 2, juce::dontSendNotification);
    tabStevie.setToggleState (t == 3, juce::dontSendNotification);
    tabPaul.setToggleState   (t == 4, juce::dontSendNotification);
    showTab (t);

    startTimerHz (4);
}

BridgeEditor::~BridgeEditor()
{
    stopTimer();
    proc.apvtsMain.state.removeListener (this);
}

void BridgeEditor::timerCallback()
{
    updateBpmDisplay();
}

void BridgeEditor::updateBpmDisplay()
{
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

    juce::TextButton* tabs[] = { &tabMain, &tabAnimal, &tabBootsy, &tabStevie, &tabPaul };
    const char* ids[] = { "leaderTabOn", "animalOn", "bootsyOn", "stevieOn", "paulOn" };

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

    juce::TextButton* tabs[] = { &tabMain, &tabAnimal, &tabBootsy, &tabStevie, &tabPaul };
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

    // Left cluster: BRIDGE logo
    logoLabel.setBounds (header.removeFromLeft (96));
    header.removeFromLeft (8);

    // Transport cluster: BPM | Play | Stop | HOST SYNC
    auto transport = header.removeFromLeft (300);
    {
        auto bpmBox = transport.removeFromLeft (74);
        bpmValueLabel.setBounds (bpmBox.removeFromLeft (44));
        bpmBox.removeFromLeft (3);
        bpmUnitLabel.setBounds (bpmBox);
        transport.removeFromLeft (10);

        const int playSide = 38;
        playButton.setBounds (transport.removeFromLeft (playSide).withSizeKeepingCentre (playSide, playSide));
        transport.removeFromLeft (6);
        stopButton.setBounds (transport.removeFromLeft (playSide).withSizeKeepingCentre (playSide, playSide));
        transport.removeFromLeft (10);

        hostSyncButton.setBounds (transport.removeFromLeft (96).withSizeKeepingCentre (96, 32));
    }

    // Right cluster: tab strip (LEADER / ANIMAL / BOOTSY / KEYS / GUITAR)
    auto tabStrip = header;
    const int tabCount = 5;
    const int tabGap   = 6;
    const int totalGap = tabGap * (tabCount - 1);
    const int tabW     = (tabStrip.getWidth() - totalGap) / tabCount;

    juce::TextButton* tabs[] = { &tabMain, &tabAnimal, &tabBootsy, &tabStevie, &tabPaul };
    for (int i = 0; i < tabCount; ++i)
    {
        tabs[i]->setBounds (tabStrip.removeFromLeft (tabW).withSizeKeepingCentre (tabW, 32));
        if (i < tabCount - 1)
            tabStrip.removeFromLeft (tabGap);
    }

    // ── Panel area (everything below the header) ───────────────────────────
    mainPanel.setBounds   (r);
    animalPanel.setBounds (r);
    bootsyPanel.setBounds (r);
    steviePanel.setBounds (r);
    paulPanel.setBounds   (r);

    updateTabStripFromParams();
}

void BridgeEditor::showTab (int index)
{
    mainPanel.setVisible   (index == 0);
    animalPanel.setVisible (index == 1);
    bootsyPanel.setVisible (index == 2);
    steviePanel.setVisible (index == 3);
    paulPanel.setVisible   (index == 4);

    if (index == 0)        mainPanel.toFront (true);
    else if (index == 1)   animalPanel.toFront (true);
    else if (index == 2)   bootsyPanel.toFront (true);
    else if (index == 3)   steviePanel.toFront (true);
    else                   paulPanel.toFront (true);

    updateTabStripFromParams();
    repaint();
}

void BridgeEditor::notifyAnimalPatternChanged()
{
    animalPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyBootsyPatternChanged()
{
    bootsyPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifySteviePatternChanged()
{
    steviePanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyPaulPatternChanged()
{
    paulPanel.triggerAsyncUpdate();
}
