#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "paul/PaulStylePresets.h"
#include "paul/PaulLookAndFeel.h"

class PaulPanel;
class FillHoldListener;

class PaulPianoRollComponent : public juce::Component
{
public:
    explicit PaulPianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;

private:
    BridgeProcessor& proc;

    static bool isBlackKey (int midiNote);
};

class PaulBassGridComponent : public juce::Component
{
public:
    PaulBassGridComponent (BridgeProcessor& p);

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized  () override;
    void update   (int activeStep);

private:
    BridgeProcessor& proc;
    int currentStep = -1;
};

class PaulLabelledKnob : public juce::Component
{
public:
    PaulLabelledKnob (const juce::String& paramId, const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PaulPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PaulPanel (BridgeProcessor&);
    ~PaulPanel () override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void handleAsyncUpdate() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void syncLockedWidthFromParams();
    void applyLoopLockAfterStartChange();
    void applyLoopLockAfterEndChange();
    static void setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value);

    void updateTickerButtonStates();
    void setTickerSpeedChoice (int index);

    BridgeProcessor& proc;
    PaulLookAndFeel laf;

    juce::OwnedArray<juce::TextButton> styleButtons;
    void updateStyleButtonStates (int active);

    PaulPianoRollComponent pianoRoll;
    PaulBassGridComponent  bassGrid;

    juce::ComboBox rootNoteBox;
    juce::Label    rootNoteLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootNoteAttach;

    juce::ComboBox scaleBox;
    juce::Label    scaleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;

    juce::ComboBox octaveBox;
    juce::Label    octaveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octaveAttach;

    PaulLabelledKnob knobDensity, knobSwing, knobHumanize;
    PaulLabelledKnob knobPocket, knobVelocity, knobComplexity;
    PaulLabelledKnob knobGhost, knobStaccato, knobFillRate;

    juce::Label    loopSectionLabel;
    PaulLabelledKnob   knobLoopStart;
    PaulLabelledKnob   knobLoopEnd;
    juce::TextButton loopWidthLockButton { "" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopWidthLockAttach;

    bool updatingLoopParams = false;
    int  lockedLoopWidth    = PaulPreset::NUM_STEPS;

    juce::Label    tickerLabel;
    juce::TextButton tickerFastButton   { "2" };
    juce::TextButton tickerNormalButton { "1" };
    juce::TextButton tickerSlowButton   { "1/2" };

    juce::Label    styleLabel;
    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton fillButton     { "FILL" };
    std::unique_ptr<FillHoldListener> fillHoldListener;

    struct StepTimer : public juce::Timer
    {
        PaulPanel& panel;
        StepTimer (PaulPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaulPanel)
};
