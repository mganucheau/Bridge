#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "MainPanel.h"
#include "AnimalPanel.h"
#include "BootsyPanel.h"
#include "SteviePanel.h"
#include "PaulPanel.h"

class BridgeEditor : public juce::AudioProcessorEditor,
                     private juce::ValueTree::Listener
{
public:
    explicit BridgeEditor (BridgeProcessor&);
    ~BridgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void notifyAnimalPatternChanged();
    void notifyBootsyPatternChanged();
    void notifySteviePatternChanged();
    void notifyPaulPatternChanged();

private:
    void showTab (int index);
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void updateTabStripFromParams();

    BridgeProcessor& proc;

    juce::TextButton powerLeader { juce::CharPointer_UTF8 ("\u25cf") };
    juce::TextButton powerAnimal { juce::CharPointer_UTF8 ("\u25cf") };
    juce::TextButton powerBootsy { juce::CharPointer_UTF8 ("\u25cf") };
    juce::TextButton powerStevie { juce::CharPointer_UTF8 ("\u25cf") };
    juce::TextButton powerPaul   { juce::CharPointer_UTF8 ("\u25cf") };

    juce::TextButton tabMain   { "Leader" };
    juce::TextButton tabAnimal { "Drums" };
    juce::TextButton tabBootsy { "Bass" };
    juce::TextButton tabStevie { "Keys" };
    juce::TextButton tabPaul   { "Guitar" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerLeaderAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAnimalAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerBootsyAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerStevieAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerPaulAttach;

    MainPanel   mainPanel;
    AnimalPanel animalPanel;
    BootsyPanel bootsyPanel;
    SteviePanel steviePanel;
    PaulPanel   paulPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
