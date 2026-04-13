#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "MainPanel.h"
#include "DrumsPanel.h"
#include "BassPanel.h"
#include "PianoPanel.h"
#include "GuitarPanel.h"

class BridgeEditor : public juce::AudioProcessorEditor,
                     private juce::ValueTree::Listener,
                     private juce::Timer
{
public:
    explicit BridgeEditor (BridgeProcessor&);
    ~BridgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void notifyDrumsPatternChanged();
    void notifyBassPatternChanged();
    void notifyPianoPatternChanged();
    void notifyGuitarPatternChanged();

    bool keyPressed (const juce::KeyPress& key) override;

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
    juce::ShapeButton gearButton { "Settings", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    juce::ShapeButton playButton { "Play", juce::Colour(0xffeae2d5), juce::Colour(0xffeae2d5), juce::Colour(0xff6ee7a0) };
    juce::ShapeButton stopButton { "Stop", juce::Colour(0xffeae2d5), juce::Colour(0xffeae2d5), juce::Colour(0xff6ee7a0) };
    juce::TextButton hostSyncButton { "HOST SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> transportAttach;

    // ── Tab strip ──────────────────────────────────────────────────────────
    juce::TextButton tabMain   { "LEADER" };
    juce::TextButton tabDrums { "DRUMS" };
    juce::TextButton tabBass { "BASS" };
    juce::TextButton tabPiano { "PIANO" };
    juce::TextButton tabGuitar   { "GUITAR" };

    MainPanel   mainPanel;
    DrumsPanel drumsPanel;
    BassPanel bassPanel;
    PianoPanel pianoPanel;
    GuitarPanel   guitarPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
