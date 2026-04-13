#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "guitar/GuitarStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeInstrumentUI.h"
#include "guitar/GuitarLookAndFeel.h"

class GuitarPanel;
class FillHoldListener;

class GuitarPianoRollComponent : public juce::Component
{
public:
    explicit GuitarPianoRollComponent (BridgeProcessor& p);
    void paint (juce::Graphics&) override;

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
    bool updatingLoopParams = false;
    

    

        GuitarPianoRollComponent pianoRoll { proc };
    GuitarGridComponent grid { proc };
    
    juce::Label    styleLabel;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    juce::Label  rootLabel;
    juce::ComboBox rootBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttach;
    
    juce::Label  scaleLabel;
    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;
    
    juce::Label  octaveLabel;
    juce::ComboBox octaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octaveAttach;


    struct StepTimer : public juce::Timer
    {
        GuitarPanel& panel;
        StepTimer (GuitarPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    void updateStepAnimation();
    int  lastAnimStep = -1;

    PagePowerButton pagePower { bridge::colors::accentGuitar };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttach;
    void applyGuitarPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarPanel)
};
