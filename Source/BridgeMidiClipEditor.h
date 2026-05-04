#pragma once

#include <functional>
#include <JuceHeader.h>
#include "BridgeProcessor.h"

/** Ableton-style clip note area: time ruler, piano/drum key strip, note blocks from BridgeClipTimeline,
    optional Fold, pinch/zoom hooks, and an internal velocity lane. */
class BridgeMidiClipEditor : public juce::Component
{
public:
    enum class InstrumentKind { drums, bass, piano, guitar };

    static constexpr int kTimeRulerHeightPx    = 18;
    static constexpr int kVelocityLaneHeightPx = 40;
    /** Ruler + velocity lane height; note grid height is separate. */
    static constexpr int verticalChromePx = kTimeRulerHeightPx + kVelocityLaneHeightPx;

    BridgeMidiClipEditor (BridgeProcessor& processor,
                          InstrumentKind instrument,
                          juce::Colour accentColour,
                          std::function<void (float)> onZoomTime,
                          std::function<void (float)> onZoomPitch);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setZoom (float zx, float zy) noexcept;
    void setVerticalScroll (int y) noexcept;
    int  getVerticalScroll() const noexcept { return scrollY; }
    void updatePlayhead (int activeStep0);

private:
    const BridgeClipTimeline& clipRef() const;
    int phraseSteps() const;
    void paintTimeRuler (juce::Graphics& g, juce::Rectangle<float> rulerArea, float pxPerStep);
    void paintKeyStrip (juce::Graphics& g, juce::Rectangle<float> stripArea, float rowH,
                        int rowLoMidi, int rowHiMidi, bool drumMode);
    void paintNotes (juce::Graphics& g, juce::Rectangle<float> gridArea, float pxPerStep, float rowH,
                     int rowLoMidi, int rowHiMidi, bool drumMode);
    void paintVelocityLane (juce::Graphics& g, juce::Rectangle<float> laneArea, float pxPerStep);
    void layoutFoldButton();

    BridgeProcessor& proc;
    InstrumentKind   kind;
    juce::Colour     accent;
    std::function<void (float)> zoomTime;
    std::function<void (float)> zoomPitch;

    juce::ToggleButton foldButton { "Fold" };
    float zoomX = 1.f;
    float zoomY = 1.f;
    int   scrollY = 0;
    int   playheadStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeMidiClipEditor)
};
