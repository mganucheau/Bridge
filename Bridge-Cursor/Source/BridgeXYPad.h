#pragma once

#include <JuceHeader.h>

/** Draggable 2-axis pad: maps normalized X/Y to two APVTS float parameters (0–1). */
class BridgeXYPad : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    BridgeXYPad (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramIdX,
                   const juce::String& paramIdY,
                   juce::Colour accentColour);
    ~BridgeXYPad() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void parameterChanged (const juce::String& id, float) override;
    void syncFromParameters();
    void setFromPosition (juce::Point<float> posInLocal);
    juce::Rectangle<float> padBounds() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String pidX;
    juce::String pidY;
    juce::Colour accent;
    bool dragging = false;
};
