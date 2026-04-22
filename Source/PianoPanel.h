#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "piano/PianoStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "InstrumentControlBar.h"
#include "piano/PianoLookAndFeel.h"
#include "piano/PianoStylePresets.h"

class PianoPanel;
class FillHoldListener;

class PianoPianoRollComponent : public juce::Component
{
public:
    explicit PianoPianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    BridgeProcessor& proc;

    static bool isBlackKey (int midiNote);
};

class PianoGridComponent : public juce::Component
{
public:
    PianoGridComponent (BridgeProcessor& p);

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

class PianoLabelledKnob : public juce::Component
{
public:
    PianoLabelledKnob (const juce::String& paramId, const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PianoPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PianoPanel (BridgeProcessor&);
    ~PianoPanel () override;

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
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xffbf5af2), PianoPreset::NUM_STEPS };

    

        PianoPianoRollComponent pianoRoll { proc };
    PianoGridComponent grid { proc };

    struct StepTimer : public juce::Timer
    {
        PianoPanel& panel;
        StepTimer (PianoPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    void applyPianoPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoPanel)
};
