#pragma once

#include <functional>
#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "piano/PianoStylePresets.h"
#include "BridgeLookAndFeel.h"
#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeVelocityStrip.h"
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
    void setCellSize (float w, float h);

private:
    BridgeProcessor& proc;
    float storedCellW = 1.0f;
    float storedCellH = 1.0f;

    static bool isBlackKey (int midiNote);
};

class PianoGridComponent : public juce::Component
{
public:
    PianoGridComponent (PianoPanel& panel, BridgeProcessor& p);

    void paint    (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void magnify (const juce::MouseEvent& e, float scaleFactor);
    void resized  () override;
    void update   (int activeStep);
    void setCellSize (float w, float h);

private:
    PianoPanel&      panel;
    BridgeProcessor& proc;
    float storedCellW = 1.0f;
    float storedCellH = 1.0f;
    int currentStep = -1;
    int dragOriginStep = -1;
};

/** Piano roll + step grid sized to the committed pattern pitch span (scrolls vertically). */
struct PianoMelodicBody : public juce::Component
{
    explicit PianoMelodicBody (PianoPanel& panel, BridgeProcessor& p);
    void resized() override;
    void setMelodicCellSize (float cellW, float cellH);

    PianoPianoRollComponent roll;
    PianoGridComponent grid;

private:
    float layoutCellW = 1.0f;
    float layoutCellH = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoMelodicBody)
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
    PianoMelodicBody melodicBody { *this, proc };
    BridgeLookAndFeel laf;
    BridgeBottomHalf bottomHalf;
    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xffbf5af2), PianoPreset::NUM_STEPS };
    BridgeVelocityStrip velocityStrip { PianoPreset::NUM_STEPS, juce::Colour (0xffbf5af2) };

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
