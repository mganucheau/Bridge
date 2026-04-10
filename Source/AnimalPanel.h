#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "animal/AnimalLookAndFeel.h"

class AnimalPanel;

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

class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (const juce::String& paramId, const juce::String& name,
                  juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class FillHoldListener;

class AnimalPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AnimalPanel (BridgeProcessor&);
    ~AnimalPanel() override;

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

    BridgeProcessor& proc;
    AnimalLookAndFeel laf;

    juce::Label    loopSectionLabel;
    LabelledKnob   knobLoopStart;
    LabelledKnob   knobLoopEnd;
    juce::TextButton loopWidthLockButton { "" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopWidthLockAttach;

    bool updatingLoopParams = false;
    int  lockedLoopWidth    = NUM_STEPS;

    DrumGridComponent drumGrid;

    LabelledKnob knobDensity, knobSwing, knobHumanize;
    LabelledKnob knobVelocity, knobFillRate, knobComplexity;
    LabelledKnob knobPocket, knobGhost;

    juce::Label    tickerLabel;
    juce::TextButton tickerFastButton   { "x2" };
    juce::TextButton tickerNormalButton { "1" };
    juce::TextButton tickerSlowButton   { "1/2" };

    juce::Label    styleLabel;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;

    juce::ComboBox rootNoteBox;
    juce::Label    rootNoteLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootNoteAttach;

    juce::ComboBox scaleBox;
    juce::Label    scaleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;

    juce::ComboBox octaveBox;
    juce::Label    octaveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octaveAttach;

    juce::TextButton generateButton { "GEN" };
    juce::TextButton performButton  { "PERF" };
    juce::TextButton fillButton     { "FILL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> performAttach;
    std::unique_ptr<FillHoldListener> fillHoldListener;

    struct StepTimer : public juce::Timer
    {
        AnimalPanel& panel;
        StepTimer (AnimalPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;
    uint32_t performBlinkTick = 0;

    void updateTickerButtonStates();
    void setTickerSpeedChoice (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnimalPanel)
};
