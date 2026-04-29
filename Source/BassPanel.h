#pragma once

#include <functional>
#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "bass/BassStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeMidiClipEditor.h"
#include "InstrumentControlBar.h"
#include "bass/BassLookAndFeel.h"

class BassPanel;

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
    int  currentPhraseBarCount() const;
    void applyPhraseBarsToUi();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassPanel)
};
