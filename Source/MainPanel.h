#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"

class MainPanel : public juce::Component,
                  private juce::Timer,
                  private juce::ValueTree::Listener
{
public:
    explicit MainPanel (BridgeProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void syncSoloButtonColours();
    void applyLeaderEngaged();

    struct LeaderKnob : public juce::Component
    {
        LeaderKnob (const juce::String& paramId, const juce::String& name,
                    juce::AudioProcessorValueTreeState& apvts);
        void resized() override;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct StripPreview : public juce::Component
    {
        enum class Kind { animal, bootsy, stevie, paul };
        StripPreview (BridgeProcessor& p, Kind k) : proc (p), kind (k) {}
        void paint (juce::Graphics& g) override;

        BridgeProcessor& proc;
        Kind kind;
    };

    struct Row
    {
        juce::Label name;
        std::unique_ptr<StripPreview> preview;
        juce::TextButton mute { "M" };
        juce::TextButton solo { "S" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach;
    };

    void setupRow (Row& row,
                   const juce::String& rowName,
                   StripPreview::Kind previewKind,
                   const char* muteId,
                   const char* soloId);

    BridgeProcessor& proc;

    juce::Label title;
    juce::Component bandControls;
    juce::Label styleLabel;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;

    std::unique_ptr<LeaderKnob> knobPresence;
    std::unique_ptr<LeaderKnob> knobTight;
    std::unique_ptr<LeaderKnob> knobUnity;
    std::unique_ptr<LeaderKnob> knobBreath;
    std::unique_ptr<LeaderKnob> knobSpark;

    juce::Component mixerArea;

    Row animal;
    Row bootsy;
    Row stevie;
    Row paul;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
