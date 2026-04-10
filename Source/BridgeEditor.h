#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "MainPanel.h"
#include "AnimalPanel.h"
#include "BootsyPanel.h"
#include "SteviePanel.h"
#include "PaulPanel.h"

class BridgeEditor : public juce::AudioProcessorEditor
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
    BridgeProcessor& proc;

    juce::TextButton tabMain   { "main" };
    juce::TextButton tabAnimal { "animal" };
    juce::TextButton tabBootsy { "bootsy" };
    juce::TextButton tabStevie { "stevie" };
    juce::TextButton tabPaul   { "paul" };

    MainPanel   mainPanel;
    AnimalPanel animalPanel;
    BootsyPanel bootsyPanel;
    SteviePanel steviePanel;
    PaulPanel   paulPanel;

    void showTab (int index); // 0 Main … 4 Paul

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
