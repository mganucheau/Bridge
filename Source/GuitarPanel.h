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
    void setCellSize (float w, float h);

private:
    BridgeProcessor& proc;
    float storedCellW = 1.0f;
    float storedCellH = 1.0f;

    static bool isBlackKey (int midiNote);
};

class GuitarGridComponent : public juce::Component
{
public:
    GuitarGridComponent (GuitarPanel& panel, BridgeProcessor& p);

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void resized  () override;
    void update   (int activeStep);
    void setCellSize (float w, float h);

private:
    GuitarPanel&      parentPanel;
    BridgeProcessor& proc;
    float storedCellW = 1.0f;
    float storedCellH = 1.0f;
    int currentStep = -1;
    int dragOriginStep = -1;
};

/** Guitar roll + step grid sized to the committed pattern pitch span (scrolls vertically). */
struct GuitarMelodicBody : public juce::Component
{
    explicit GuitarMelodicBody (GuitarPanel& panel, BridgeProcessor& p);
    void resized() override;
    void setMelodicCellSize (float cellW, float cellH);

    GuitarPianoRollComponent roll;
    GuitarGridComponent grid;

private:
    float layoutCellW = 1.0f;
    float layoutCellH = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarMelodicBody)
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

    float zoomX = 1.0f;
    float zoomY = 1.0f;

    void adjustZoomX (float wheelDeltaY);
    void adjustZoomY (float wheelDeltaY);
    void fitPatternInView();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

                static void setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value);

    BridgeProcessor& proc;
    GuitarMelodicBody melodicBody { *this, proc };
    BridgeLookAndFeel laf;
    BridgeBottomHalf bottomHalf;
    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xff0a84ff), GuitarPreset::NUM_STEPS };

    juce::Viewport melodicViewport;

    struct StepTimer : public juce::Timer
    {
        GuitarPanel& panel;
        StepTimer (GuitarPanel& p) : panel (p) {}
        void timerCallback() override
        {
            panel.flushDeferredGuitarGridPreviewRebuild();
            panel.updateStepAnimation();
        }
    } stepTimer { *this };

    void updateStepAnimation();
    void flushDeferredGuitarGridPreviewRebuild();
    int  lastAnimStep = -1;

    bool deferredGuitarGridPreviewRebuild = false;

    void applyGuitarPageState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarPanel)
};
