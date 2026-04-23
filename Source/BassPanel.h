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

/** Bass roll + step grid sized to the committed pattern pitch span (scrolls vertically). */
struct BassMelodicBody : public juce::Component
{
    explicit BassMelodicBody (BridgeProcessor& p);
    void resized() override;

    BassPianoRollComponent roll;
    BassGridComponent grid;

private:
    BridgeProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassMelodicBody)
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
    BassMelodicBody melodicBody { proc };
    BridgeLookAndFeel laf;
    BridgeBottomHalf bottomHalf;
    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xff5ed4c4), BassPreset::NUM_STEPS };

    juce::Viewport melodicViewport;

    struct StepTimer : public juce::Timer
    {
        BassPanel& panel;
        StepTimer (BassPanel& p) : panel (p) {}
        void timerCallback() override
        {
            panel.flushDeferredBassGridPreviewRebuild();
            panel.updateStepAnimation();
        }
    } stepTimer { *this };

    void updateStepAnimation();
    void flushDeferredBassGridPreviewRebuild();
    int  lastAnimStep = -1;

    bool deferredBassGridPreviewRebuild = false;

    void applyBassPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassPanel)
};
