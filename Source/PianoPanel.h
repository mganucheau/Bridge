#pragma once

#include <functional>
#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "piano/PianoStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeMidiClipEditor.h"
#include "InstrumentControlBar.h"
#include "piano/PianoLookAndFeel.h"
#include "piano/PianoStylePresets.h"

class PianoPanel;

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

    float zoomX = 1.0f;
    float zoomY = 1.0f;

    void adjustZoomX (float delta);
    void adjustZoomY (float delta);
    void fitPatternInView();

private:
    void scrollMelodicViewportToPatternCentre();
    std::function<void()> melodicTonalityPrev;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

                static void setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value);

    BridgeProcessor& proc;
    BridgeMidiClipEditor midiClipEditor;
    BridgeLookAndFeel laf;
    BridgeBottomHalf bottomHalf;
    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xffbf5af2), PianoPreset::NUM_STEPS };

    juce::Viewport melodicViewport;

    struct StepTimer : public juce::Timer
    {
        PianoPanel& panel;
        StepTimer (PianoPanel& p) : panel (p) {}
        void timerCallback() override
        {
            panel.flushDeferredPianoGridPreviewRebuild();
            panel.updateStepAnimation();
        }
    } stepTimer { *this };

    void updateStepAnimation();
    void flushDeferredPianoGridPreviewRebuild();
    int  lastAnimStep = -1;

    bool deferredPianoGridPreviewRebuild = false;

    void applyPianoPageState();
    int  currentPhraseBarCount() const;
    void applyPhraseBarsToUi();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoPanel)
};
