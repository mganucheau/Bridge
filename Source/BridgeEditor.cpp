#include "BridgeEditor.h"

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

    const int t = juce::jlimit (0, 4, proc.activeTab.load());
    tabMain.setToggleState   (t == 0, juce::dontSendNotification);
    tabAnimal.setToggleState (t == 1, juce::dontSendNotification);
    tabBootsy.setToggleState (t == 2, juce::dontSendNotification);
    tabStevie.setToggleState (t == 3, juce::dontSendNotification);
    tabPaul.setToggleState   (t == 4, juce::dontSendNotification);
    showTab (t);
}

BridgeEditor::~BridgeEditor() = default;

void BridgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1820));
}

void BridgeEditor::resized()
{
    auto r = getLocalBounds();
    auto tabBar = r.removeFromTop (44);
    const int w = tabBar.getWidth() / 5;
    tabMain.setBounds   (tabBar.removeFromLeft (w).reduced (4, 6));
    tabAnimal.setBounds (tabBar.removeFromLeft (w).reduced (4, 6));
    tabBootsy.setBounds (tabBar.removeFromLeft (w).reduced (4, 6));
    tabStevie.setBounds (tabBar.removeFromLeft (w).reduced (4, 6));
    tabPaul.setBounds   (tabBar.reduced (4, 6));

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
