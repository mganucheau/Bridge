#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"

class MainPanel : public juce::Component
{
public:
    explicit MainPanel (BridgeProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BridgeProcessor& proc;

    juce::Label title { {}, "Main" };
    juce::Label subtitle;

    struct Row
    {
        juce::Label     name;
        juce::Label     hint;
        juce::ToggleButton toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
    };

    Row animalRow;
    Row bootsyRow;
    Row stevieRow;
    Row paulRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
