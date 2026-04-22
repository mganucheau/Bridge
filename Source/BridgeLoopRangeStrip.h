#pragma once

#include <JuceHeader.h>

/** Visual 1…N step loop region with draggable start (▶) and end (◀) handles. */
class BridgeLoopRangeStrip : public juce::Component
{
public:
    BridgeLoopRangeStrip (juce::AudioProcessorValueTreeState& apvts,
                          juce::Colour accentColour,
                          int numSteps = 16);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setAccent (juce::Colour c) { accent = c; repaint(); }
    /** Reserve this many pixels on the left (piano / drum lane column); step labels align from here. */
    void setStepLabelGutter (int px) { stepLabelGutterLeft = px; repaint(); }

private:
    static int readIntParam (juce::AudioProcessorValueTreeState& ap, const juce::String& id, int fallback);
    static void writeIntParam (juce::AudioProcessorValueTreeState& ap, const juce::String& id, int v, int maxStep);

    int  xToStep (float x) const;
    void syncFromMouse (const juce::MouseEvent& e, bool isDragging);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;
    int numSteps = 16;
    int stepLabelGutterLeft = 0;
    int dragMode = 0; // 0 none, 1 start, 2 end
};
