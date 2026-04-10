#include "BridgeEditor.h"

namespace
{
static bool paramOn (juce::AudioProcessorValueTreeState& ap, const char* id)
{
    if (auto* v = ap.getRawParameterValue (id))
        return v->load() > 0.5f;
    return true;
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
    setSize (960, 740);
    setResizable (false, false);

    auto setupPower = [] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8e8f0));
    };

    setupPower (powerLeader);
    setupPower (powerAnimal);
    setupPower (powerBootsy);
    setupPower (powerStevie);
    setupPower (powerPaul);

    tabMain.setClickingTogglesState (true);
    tabAnimal.setClickingTogglesState (true);
    tabBootsy.setClickingTogglesState (true);
    tabStevie.setClickingTogglesState (true);
    tabPaul.setClickingTogglesState (true);
    tabMain.setRadioGroupId (42);
    tabAnimal.setRadioGroupId (42);
    tabBootsy.setRadioGroupId (42);
    tabStevie.setRadioGroupId (42);
    tabPaul.setRadioGroupId (42);

    tabMain.onClick = [this]
    {
        proc.activeTab.store (0);
        showTab (0);
    };
    tabAnimal.onClick = [this]
    {
        proc.activeTab.store (1);
        showTab (1);
    };
    tabBootsy.onClick = [this]
    {
        proc.activeTab.store (2);
        showTab (2);
    };
    tabStevie.onClick = [this]
    {
        proc.activeTab.store (3);
        showTab (3);
    };
    tabPaul.onClick = [this]
    {
        proc.activeTab.store (4);
        showTab (4);
    };

    addAndMakeVisible (powerLeader);
    addAndMakeVisible (powerAnimal);
    addAndMakeVisible (powerBootsy);
    addAndMakeVisible (powerStevie);
    addAndMakeVisible (powerPaul);

    addAndMakeVisible (tabMain);
    addAndMakeVisible (tabAnimal);
    addAndMakeVisible (tabBootsy);
    addAndMakeVisible (tabStevie);
    addAndMakeVisible (tabPaul);

    addChildComponent (mainPanel);
    addChildComponent (animalPanel);
    addChildComponent (bootsyPanel);
    addChildComponent (steviePanel);
    addChildComponent (paulPanel);

    powerLeaderAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "leaderTabOn", powerLeader);
    powerAnimalAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "animalOn", powerAnimal);
    powerBootsyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "bootsyOn", powerBootsy);
    powerStevieAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "stevieOn", powerStevie);
    powerPaulAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "paulOn", powerPaul);

    proc.apvtsMain.state.addListener (this);

    const int t = juce::jlimit (0, 4, proc.activeTab.load());
    tabMain.setToggleState   (t == 0, juce::dontSendNotification);
    tabAnimal.setToggleState (t == 1, juce::dontSendNotification);
    tabBootsy.setToggleState (t == 2, juce::dontSendNotification);
    tabStevie.setToggleState (t == 3, juce::dontSendNotification);
    tabPaul.setToggleState   (t == 4, juce::dontSendNotification);
    showTab (t);
    updateTabStripFromParams();
}

BridgeEditor::~BridgeEditor()
{
    proc.apvtsMain.state.removeListener (this);
}

void BridgeEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    updateTabStripFromParams();
}

void BridgeEditor::updateTabStripFromParams()
{
    const juce::Colour cLeader (0xffd4a84b);
    const juce::Colour cAnimal (0xffe07a5a);
    const juce::Colour cBootsy (0xff5cb8a8);
    const juce::Colour cStevie (0xffb88cff);
    const juce::Colour cPaul (0xff6eb3ff);
    const juce::Colour offBg (0xff2a2830);
    const juce::Colour offTx (0xff6a6578);

    auto stylePower = [&] (juce::TextButton& b, bool on, juce::Colour c)
    {
        if (on)
        {
            b.setColour (juce::TextButton::buttonColourId, c.withAlpha (0.55f));
            b.setColour (juce::TextButton::textColourOffId, c.brighter (0.15f));
        }
        else
        {
            b.setColour (juce::TextButton::buttonColourId, offBg);
            b.setColour (juce::TextButton::textColourOffId, offTx);
        }
    };

    stylePower (powerLeader, paramOn (proc.apvtsMain, "leaderTabOn"), cLeader);
    stylePower (powerAnimal, paramOn (proc.apvtsMain, "animalOn"), cAnimal);
    stylePower (powerBootsy, paramOn (proc.apvtsMain, "bootsyOn"), cBootsy);
    stylePower (powerStevie, paramOn (proc.apvtsMain, "stevieOn"), cStevie);
    stylePower (powerPaul, paramOn (proc.apvtsMain, "paulOn"), cPaul);

    tabMain.setAlpha   (paramOn (proc.apvtsMain, "leaderTabOn") ? 1.0f : 0.42f);
    tabAnimal.setAlpha (paramOn (proc.apvtsMain, "animalOn") ? 1.0f : 0.42f);
    tabBootsy.setAlpha (paramOn (proc.apvtsMain, "bootsyOn") ? 1.0f : 0.42f);
    tabStevie.setAlpha (paramOn (proc.apvtsMain, "stevieOn") ? 1.0f : 0.42f);
    tabPaul.setAlpha   (paramOn (proc.apvtsMain, "paulOn") ? 1.0f : 0.42f);
}

void BridgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1820));
}

void BridgeEditor::resized()
{
    auto r = getLocalBounds();
    auto tabBar = r.removeFromTop (44);
    const int cols = 5;
    const int pw = 26;
    const int colW = tabBar.getWidth() / cols;

    juce::TextButton* powers[] = { &powerLeader, &powerAnimal, &powerBootsy, &powerStevie, &powerPaul };
    juce::TextButton* tabs[]   = { &tabMain, &tabAnimal, &tabBootsy, &tabStevie, &tabPaul };

    for (int i = 0; i < cols; ++i)
    {
        auto cell = tabBar.removeFromLeft (colW);
        powers[i]->setBounds (cell.removeFromLeft (pw).reduced (2, 8));
        tabs[i]->setBounds (cell.reduced (4, 6));
    }

    mainPanel.setBounds   (r);
    animalPanel.setBounds (r);
    bootsyPanel.setBounds (r);
    steviePanel.setBounds (r);
    paulPanel.setBounds   (r);
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
