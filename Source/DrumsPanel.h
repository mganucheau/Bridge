#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeLookAndFeel.h"
#include "BridgeMainLoopKnobAttachment.h"
#include "BridgeVelocityStrip.h"
#include "BridgeXYPad.h"
#include "InstrumentControlBar.h"
#include "drums/DrumsStylePresets.h"
#include "drums/DrumsLookAndFeel.h"
#include "BridgePhrase.h"

class DrumGridComponent : public juce::Component
{
public:
    DrumGridComponent (BridgeProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized() override;
    void update (int activeStep);

    /** PatternFlow-style phrase length: how many bars the grid visualises. cellW is recomputed
        so the full N-bar grid fits the same container width. The underlying engine pattern is
        always 1 bar (NUM_STEPS) and is tiled across the visible bars. Default 1. */
    void setBarCount (int bars);
    int  getBarCount() const noexcept { return barCount; }

private:
    BridgeProcessor& proc;
    int currentStep = -1;
    int barCount    = 1;

    float geomLaneX = 0.0f, geomLaneW = 0.0f, geomOriginX = 0.0f, geomOriginY = 0.0f, geomCellW = 0.0f, geomRowH = 0.0f;
    void syncGeometryFromBounds();

    juce::OwnedArray<juce::TextButton> muteButtons;
    juce::OwnedArray<juce::TextButton> soloButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachments;

    static int visualRowToDrum (int visualRow);
};


class FillHoldListener;

class DrumsPanel  : public juce::Component,
                     public juce::AsyncUpdater,
                     private juce::ValueTree::Listener,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrumsPanel (BridgeProcessor&);
    ~DrumsPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void handleAsyncUpdate() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void layoutBottomControls (juce::Rectangle<int> bottom);

                static void setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& id, int value);

    BridgeProcessor& proc;
    BridgeLookAndFeel laf;

    InstrumentControlBar instrumentStrip;
    BridgeLoopRangeStrip loopStrip { proc.apvtsMain, juce::Colour (0xffff7f5c), bridge::phrase::kMaxSteps };
    DrumGridComponent drumGrid;
    BridgeVelocityStrip velocityStrip { bridge::phrase::kMaxSteps, juce::Colour (0xffff7f5c) };

    // ── Bottom controls (laid out by DrumsPanel::resized) ─────────────────────
    juce::Slider knobSwing,    knobHumanize, knobHold, knobVelocity;
    juce::Label  labelSwing,   labelHumanize, labelHold, labelVelocity;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attSwing, attHumanize, attHold, attVelocity;

    juce::Label  patternHeader, fillsHeader;
    juce::Label  patternXLabel, patternYLabel;
    juce::Label  fillsXLabel,   fillsYLabel;
    std::unique_ptr<BridgeXYPad> patternPad;
    std::unique_ptr<BridgeXYPad> fillsPad;

    juce::Slider patternKnobX, patternKnobY;
    juce::Slider fillsKnobX, fillsKnobY;
    juce::Label  patternKnobXLabel, patternKnobYLabel;
    juce::Label  fillsKnobXLabel, fillsKnobYLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attPatternKnobX, attPatternKnobY, attFillsKnobX, attFillsKnobY;

    juce::Slider knobLoopStart, knobLoopEnd;
    juce::Label  labelLoopStart, labelLoopEnd;
    std::unique_ptr<bridge::MainLoopKnobSliderAttachment> mainLoopKnobStartAttach;
    std::unique_ptr<bridge::MainLoopKnobSliderAttachment> mainLoopKnobEndAttach;
    juce::TextButton loopPlaybackButton;
    juce::TextButton syncIconButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attLoopPlayback, attLoopSpan;
    juce::Label selectorsHeader, actionsHeader;

    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton jamToggle;
    juce::ComboBox     jamPeriodBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> jamToggleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> jamPeriodAttach;
    juce::Label      jamLabel;

    struct StepTimer : public juce::Timer
    {
        DrumsPanel& panel;
        StepTimer (DrumsPanel& p) : panel (p) {}
        void timerCallback() override { panel.updateStepAnimation(); }
    } stepTimer { *this };

    struct DensityComplexityDebounce : public juce::Timer
    {
        DrumsPanel& p;
        explicit DensityComplexityDebounce (DrumsPanel& o) : p (o) {}
        void timerCallback() override;
    } densityComplexityDebounce { *this };

    void performDebouncedDrumRegenerate();

    void updateStepAnimation();
    int  lastAnimStep = -1;

    void applyDrumsPageState();
    /** Read the global phraseBars choice on apvtsMain (0 → 2 bars, 1 → 4, 2 → 8). */
    int  currentPhraseBarCount() const;
    /** Push the current phrase length to grid + loop strip + velocity strip + engine. */
    void applyPhraseBars();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumsPanel)
};
