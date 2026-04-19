#pragma once

#include "BridgeLookAndFeel.h"

class BridgeLoopRangeStrip;

class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (const juce::String& paramId, const juce::String& name,
                  juce::AudioProcessorValueTreeState& apvts, BridgeLookAndFeel::KnobStyle style, juce::Colour accent);
    void resized() override;
    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class BridgeBottomHalf : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    BridgeBottomHalf (juce::AudioProcessorValueTreeState& apvts,
                      BridgeLookAndFeel& laf,
                      juce::Colour groupAccent,
                      std::function<void()> onGenerate,
                      std::function<void(bool)> onFillHold);

    ~BridgeBottomHalf() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour groupAccentColour;

    // GROOVE
    juce::Label grooveLabel;
    LabelledKnob knobDensity;
    LabelledKnob knobSwing;
    LabelledKnob knobHumanize;
    LabelledKnob knobPocket;
    LabelledKnob knobVelocity;

    // EXPRESSION (4 Knobs)
    juce::Label expressionLabel;
    LabelledKnob knobFillRate;
    LabelledKnob knobComplexity;
    LabelledKnob knobGhost;
    LabelledKnob knobPresence;

    // LOOPING
    juce::Label loopingLabel;
    std::unique_ptr<BridgeLoopRangeStrip> loopRangeStrip;
    LabelledKnob knobLoopStart;
    LabelledKnob knobLoopEnd;
    juce::ShapeButton syncIconButton { "Sync", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    // ACTIONS
    juce::Label actionsLabel;
    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton fillButton { "FILL" };
    juce::TextButton performButton { "JAM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> performAttach;

    std::function<void(bool)> fillHoldCallback;

    class FillMouseListener : public juce::MouseListener
    {
    public:
        FillMouseListener (BridgeBottomHalf& o) : owner(o) {}
        void mouseDown (const juce::MouseEvent& e) override { if (e.eventComponent == &owner.fillButton && owner.fillHoldCallback) owner.fillHoldCallback(true); }
        void mouseUp   (const juce::MouseEvent& e) override { if (e.eventComponent == &owner.fillButton && owner.fillHoldCallback) owner.fillHoldCallback(false); }
    private:
        BridgeBottomHalf& owner;
    } fillListener { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeBottomHalf)
};
