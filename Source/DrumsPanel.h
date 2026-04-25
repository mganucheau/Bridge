#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeLookAndFeel.h"
#include "BridgeVelocityStrip.h"
#include "InstrumentControlBar.h"
#include "drums/DrumsStylePresets.h"
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

    float geomLaneX = 0.0f, geomLaneW = 0.0f, geomOriginX = 0.0f, geomOriginY = 0.0f, geomCellW = 0.0f, geomRowH = 0.0f;
    void syncGeometryFromBounds();

    juce::OwnedArray<juce::TextButton> muteButtons;
    juce::OwnedArray<juce::TextButton> soloButtons;
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
    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xffff7f5c), NUM_STEPS };
    DrumGridComponent drumGrid;
    BridgeVelocityStrip velocityStrip { NUM_STEPS, juce::Colour (0xffff7f5c) };

    struct StepTimer : public juce::Timer
    {
        DrumsPanel& panel;
        StepTimer (DrumsPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    void applyDrumsPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumsPanel)
};
