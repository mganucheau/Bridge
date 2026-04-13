#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeBottomHalf.h"
#include "BridgeLookAndFeel.h"
#include "BridgeInstrumentUI.h"
#include "drums/DrumsLookAndFeel.h"

class DrumGridComponent : public juce::Component
{
public:
    DrumGridComponent (BridgeProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized() override;
    void update (int activeStep);

private:
    BridgeProcessor& proc;
    int currentStep = -1;

    juce::OwnedArray<juce::ToggleButton> muteButtons;
    juce::OwnedArray<juce::ToggleButton> soloButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachments;

    static int visualRowToDrum (int visualRow);
};


class FillHoldListener;

class DrumsPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrumsPanel (BridgeProcessor&);
    ~DrumsPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void handleAsyncUpdate() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

                static void setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value);

    BridgeProcessor& proc;
    BridgeLookAndFeel laf;


    BridgeBottomHalf bottomHalf;
    DrumGridComponent drumGrid;

    bool updatingLoopParams = false;
    int  lockedLoopWidth    = NUM_STEPS;
    
    juce::Label    styleLabel;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;


    struct StepTimer : public juce::Timer
    {
        DrumsPanel& panel;
        StepTimer (DrumsPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;
    uint32_t performBlinkTick = 0;

    PagePowerButton pagePower { bridge::colors::accentDrums };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttach;
    void applyDrumsPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumsPanel)
};
