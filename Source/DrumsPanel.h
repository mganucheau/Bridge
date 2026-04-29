#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgeLookAndFeel.h"
#include "BridgeMidiClipEditor.h"
#include "BridgeXYPad.h"
#include "InstrumentControlBar.h"
#include "drums/DrumsStylePresets.h"
#include "drums/DrumsLookAndFeel.h"
#include "BridgePhrase.h"

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

    float zoomX = 1.0f;
    float zoomY = 1.0f;
    void adjustZoomX (float delta);
    void adjustZoomY (float delta);

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
    BridgeMidiClipEditor midiClipEditor;
    juce::Viewport melodicViewport;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attLoopStart, attLoopEnd;
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
    int  currentPhraseBarCount() const;
    void applyPhraseBars();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumsPanel)
};
