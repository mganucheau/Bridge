#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "bass/BassStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "InstrumentControlBar.h"
#include "bass/BassLookAndFeel.h"

class BassPanel;
class FillHoldListener;

class BassPianoRollComponent : public juce::Component
{
public:
    explicit BassPianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    BridgeProcessor& proc;

    static bool isBlackKey (int midiNote);
};

class BassGridComponent : public juce::Component
{
public:
    BassGridComponent (BridgeProcessor& p);

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

class BassLabelledKnob : public juce::Component
{
public:
    BassLabelledKnob (const juce::String& paramId, const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class BassPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BassPanel (BridgeProcessor&);
    ~BassPanel () override;

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
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xff5ed4c4), BassPreset::NUM_STEPS };

    BassPianoRollComponent pianoRoll { proc };
    BassGridComponent grid { proc };

    struct StepTimer : public juce::Timer
    {
        BassPanel& panel;
        StepTimer (BassPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    void applyBassPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassPanel)
};
