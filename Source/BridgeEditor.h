#pragma once

#include <memory>

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "MainPanel.h"
#include "DrumsPanel.h"
#include "BassPanel.h"
#include "PianoPanel.h"
#include "GuitarPanel.h"

class BridgeHeaderBar;
class BridgeArrangementStrip;

class BridgeEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit BridgeEditor (BridgeProcessor&);
    ~BridgeEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void notifyDrumsPatternChanged();
    void notifyBassPatternChanged();
    void notifyPianoPatternChanged();
    void notifyGuitarPatternChanged();

    bool keyPressed (const juce::KeyPress& key) override;

private:
    void showTab (int index);
    void activateTab (int tabIndex);
    void syncTabTileSelection();
    void bringHeaderToFront();
    void timerCallback() override;
    void updateBpmDisplay();

    BridgeProcessor& proc;

    /** Full 54px top bar (branding, transport, tabs, settings). */
    std::unique_ptr<BridgeHeaderBar> headerBar;
    /** Song section selector directly under the header (Intro … Outro). */
    std::unique_ptr<BridgeArrangementStrip> arrangementStrip;

    MainPanel   mainPanel;
    DrumsPanel drumsPanel;
    BassPanel bassPanel;
    PianoPanel pianoPanel;
    GuitarPanel   guitarPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
