#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "MainPanel.h"
#include "AnimalPanel.h"
#include "BootsyPanel.h"
#include "SteviePanel.h"
#include "PaulPanel.h"

class BridgeEditor : public juce::AudioProcessorEditor,
                     private juce::ValueTree::Listener,
                     private juce::Timer
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
    void timerCallback() override;
    void updateTabStripFromParams();
    void updateBpmDisplay();

    BridgeProcessor& proc;

    // ── Header transport ───────────────────────────────────────────────────
    juce::Label      logoLabel;
    juce::Label      bpmValueLabel;
    juce::Label      bpmUnitLabel;
    juce::TextButton playButton  { juce::CharPointer_UTF8 ("\u25B6") };
    juce::TextButton stopButton  { juce::CharPointer_UTF8 ("\u25A0") };
    juce::TextButton hostSyncButton { "HOST SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> transportAttach;

    // ── Tab strip ──────────────────────────────────────────────────────────
    juce::TextButton tabMain   { "LEADER" };
    juce::TextButton tabAnimal { "ANIMAL" };
    juce::TextButton tabBootsy { "BOOTSY" };
    juce::TextButton tabStevie { "KEYS" };
    juce::TextButton tabPaul   { "GUITAR" };

    MainPanel   mainPanel;
    AnimalPanel animalPanel;
    BootsyPanel bootsyPanel;
    SteviePanel steviePanel;
    PaulPanel   paulPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
