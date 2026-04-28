#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

class BridgeProcessor;

/** Five song-section controls + transition progress bar (PRIORITY 8). */
class BridgeArrangementStrip final : public juce::Component,
                                      private juce::AudioProcessorValueTreeState::Listener,
                                      private juce::Timer
{
public:
    explicit BridgeArrangementStrip (BridgeProcessor&);
    ~BridgeArrangementStrip() override;

    void resized() override;
    void paintOverChildren (juce::Graphics& g) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void syncFromApvts();

    BridgeProcessor& proc;
    std::array<std::unique_ptr<juce::TextButton>, 5> sectionBtns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeArrangementStrip)
};
