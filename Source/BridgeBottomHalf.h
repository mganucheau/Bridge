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
    /** instApvts: per-instrument params (groove, expression, perform). mainApvts: global loop + playback. */
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
    LabelledKnob knobPocket;
    LabelledKnob knobVelocity;

    juce::Label expressionLabel;
    LabelledKnob knobFillRate;
    LabelledKnob knobComplexity;
    LabelledKnob knobPresence;

    juce::Label tensionLabel;
    std::unique_ptr<BridgeXYPad> xyTension;

    juce::Label feelLabel;
    std::unique_ptr<BridgeXYPad> xyFeel;

    juce::Label selectorsLabel;
    LabelledKnob knobLoopStart;
    LabelledKnob knobLoopEnd;
    juce::ShapeButton loopPlaybackButton { "Loop", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopPlaybackAttach;
    juce::ShapeButton syncIconButton { "Sync", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    juce::Label actionsLabel;
    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton fillButton { "FILL" };
    juce::TextButton performButton { "JAM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> performAttach;

    std::function<void(bool)> fillHoldCallback;

    class FillMouseListener : public juce::MouseListener
    {
    public:
        FillMouseListener (BridgeBottomHalf& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.eventComponent == &owner.fillButton && owner.fillHoldCallback)
                owner.fillHoldCallback (true);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.eventComponent == &owner.fillButton && owner.fillHoldCallback)
                owner.fillHoldCallback (false);
        }

    private:
        BridgeBottomHalf& owner;
    } fillListener { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeBottomHalf)
};
