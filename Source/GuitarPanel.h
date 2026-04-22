#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "guitar/GuitarStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "InstrumentControlBar.h"
#include "guitar/GuitarLookAndFeel.h"
#include "guitar/GuitarStylePresets.h"

class GuitarPanel;
class FillHoldListener;

class GuitarPianoRollComponent : public juce::Component
{
public:
    explicit GuitarPianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    BridgeProcessor& proc;

    static bool isBlackKey (int midiNote);
};

class GuitarGridComponent : public juce::Component
{
public:
    GuitarGridComponent (BridgeProcessor& p);

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void resized  () override;
    void update   (int activeStep);

private:
    BridgeProcessor& proc;
    int currentStep = -1;
    int dragOriginStep = -1;
};

class GuitarLabelledKnob : public juce::Component
{
public:
    GuitarLabelledKnob (const juce::String& paramId, const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class GuitarPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit GuitarPanel (BridgeProcessor&);
    ~GuitarPanel () override;

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
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xff0a84ff), GuitarPreset::NUM_STEPS };
    bool updatingLoopParams = false;
    

    

        GuitarPianoRollComponent pianoRoll { proc };
    GuitarGridComponent grid { proc };

    struct StepTimer : public juce::Timer
    {
        GuitarPanel& panel;
        StepTimer (GuitarPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    void applyGuitarPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarPanel)
};
