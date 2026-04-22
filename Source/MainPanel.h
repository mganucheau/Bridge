#pragma once

#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeBottomHalf.h"
#include "BridgeLookAndFeel.h"
#include "InstrumentControlBar.h"

class MainPanel : public juce::Component,
                  private juce::Timer,
                  private juce::ValueTree::Listener,
                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MainPanel (BridgeProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void(int)> onOpenTab;

private:
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void syncSoloButtonColours();
    void syncPageSoloToggle();
    void applyLeaderPageState();

    struct StripPreview : public juce::Component
    {
        enum class Kind { drums, bass, piano, guitar };
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
        juce::Rectangle<int> bounds;
    };

    void setupRow (Row& row,
                   const juce::String& rowName,
                   StripPreview::Kind previewKind,
                   const char* muteId,
                   const char* soloId);

    BridgeProcessor& proc;
    BridgeLookAndFeel laf;

    InstrumentControlBar instrumentStrip;

    Row drumsRow;
    Row bassRow;
    Row pianoRow;
    Row guitarRow;

    BridgeBottomHalf bottomHalf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
