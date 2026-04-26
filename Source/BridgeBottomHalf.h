#pragma once

#include "BridgeLookAndFeel.h"
#include "BridgeXYPad.h"

class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (const juce::String& paramId, const juce::String& name,
                  juce::AudioProcessorValueTreeState& apvts, BridgeLookAndFeel::KnobStyle style, juce::Colour accent,
                  int rotaryDiameter = 48,
                  int labelBandHeight = 16);
    void resized() override;
    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    juce::Label label;
    int rotaryDiameter = 48;
    int labelBandHeight = 16;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class BridgeBottomHalf : public juce::Component
{
public:
    /** instApvts: per-instrument params (groove, expression, jam). mainApvts: global loop + playback. */
    BridgeBottomHalf (juce::AudioProcessorValueTreeState& instApvts,
                      juce::AudioProcessorValueTreeState& mainApvts,
                      BridgeLookAndFeel& laf,
                      juce::Colour groupAccent,
                      std::function<void()> onGenerate,
                      std::function<void(bool)> onFillHold);

    ~BridgeBottomHalf() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& instApvts;
    juce::AudioProcessorValueTreeState& mainApvts;

    juce::Label grooveLabel;
    LabelledKnob knobDensity;
    LabelledKnob knobSwing;
    LabelledKnob knobHumanize;
    LabelledKnob knobHold;
    LabelledKnob knobVelocity;

    juce::Label expressionLabel;
    LabelledKnob knobFillRate;
    LabelledKnob knobComplexity;
    LabelledKnob knobSustain;

    juce::Label tensionLabel;
    std::unique_ptr<BridgeXYPad> xyTension;

    juce::Label feelLabel;
    std::unique_ptr<BridgeXYPad> xyFeel;

    juce::Label selectorsLabel;
    LabelledKnob knobLoopStart;
    LabelledKnob knobLoopEnd;
    juce::TextButton loopPlaybackButton { "L" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopPlaybackAttach;
    juce::TextButton syncIconButton { "S" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    juce::Label actionsLabel;
    juce::TextButton generateButton { "GENERATE" };
    juce::ToggleButton jamToggle;
    juce::ComboBox     jamPeriodBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> jamToggleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> jamPeriodAttach;
    juce::Label jamLabel;

    std::function<void(bool)> fillHoldCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeBottomHalf)
};
