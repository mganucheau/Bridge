#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "stevie/StevieStylePresets.h"
#include "stevie/StevieLookAndFeel.h"

class SteviePanel;
class FillHoldListener;

class SteviePianoRollComponent : public juce::Component
{
public:
    explicit SteviePianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;

private:
    BridgeProcessor& proc;

    static bool isBlackKey (int midiNote);
};

class StevieBassGridComponent : public juce::Component
{
public:
    StevieBassGridComponent (BridgeProcessor& p);

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized  () override;
    void update   (int activeStep);

private:
    BridgeProcessor& proc;
    int currentStep = -1;
};

class StevieLabelledKnob : public juce::Component
{
public:
    StevieLabelledKnob (const juce::String& paramId, const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class SteviePanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SteviePanel (BridgeProcessor&);
    ~SteviePanel () override;

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
    StevieLookAndFeel laf;

    juce::OwnedArray<juce::TextButton> styleButtons;
    void updateStyleButtonStates (int active);

    SteviePianoRollComponent pianoRoll;
    StevieBassGridComponent  bassGrid;

    juce::ComboBox rootNoteBox;
    juce::Label    rootNoteLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootNoteAttach;

    juce::ComboBox scaleBox;
    juce::Label    scaleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;

    juce::ComboBox octaveBox;
    juce::Label    octaveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octaveAttach;

    StevieLabelledKnob knobDensity, knobSwing, knobHumanize;
    StevieLabelledKnob knobPocket, knobVelocity, knobComplexity;
    StevieLabelledKnob knobGhost, knobStaccato, knobFillRate;

    juce::Label    loopSectionLabel;
    StevieLabelledKnob   knobLoopStart;
    StevieLabelledKnob   knobLoopEnd;
    juce::TextButton loopWidthLockButton { "" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopWidthLockAttach;

    bool updatingLoopParams = false;
    int  lockedLoopWidth    = SteviePreset::NUM_STEPS;

    juce::Label    tickerLabel;
    juce::TextButton tickerFastButton   { "x2" };
    juce::TextButton tickerNormalButton { "1" };
    juce::TextButton tickerSlowButton   { "1/2" };

    juce::Label    styleLabel;
    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton fillButton     { "FILL" };
    std::unique_ptr<FillHoldListener> fillHoldListener;

    struct StepTimer : public juce::Timer
    {
        SteviePanel& panel;
        StepTimer (SteviePanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteviePanel)
};
