#pragma once

#include <JuceHeader.h>

class BridgeProcessor;

/** Second chrome row below the main header: Style / (melodic) Root·Scale·Oct + page M/S. */
class InstrumentControlBar : public juce::Component
{
public:
    struct Config
    {
        /** When false, only Style + M/S (drums). */
        bool melodic = true;

        juce::AudioProcessorValueTreeState* styleApvts = nullptr;
        juce::String styleParamId;

        juce::AudioProcessorValueTreeState* tonalApvts = nullptr;
        juce::String rootParamId;
        juce::String scaleParamId;
        juce::String octaveParamId;

        int numScales = 0;
        const char* const* scaleNames = nullptr;

        juce::AudioProcessorValueTreeState* soloApvts = nullptr;
        /** Empty = no attachment (caller wires solo manually, e.g. Leader tab). */
        juce::String soloParamId;
    };

    static Config makeLeaderConfig (BridgeProcessor& p);
    static Config makeDrumsConfig (BridgeProcessor& p);
    static Config makeBassConfig (BridgeProcessor& p);
    static Config makePianoConfig (BridgeProcessor& p);
    static Config makeGuitarConfig (BridgeProcessor& p);

    explicit InstrumentControlBar (const Config& cfg);
    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Mute shows “off” when the track/tab is inactive (`setToggleState (!on)`). */
    void setTrackPowered (bool on);

    juce::TextButton& getMuteButton() noexcept { return pageMute; }
    juce::TextButton& getSoloButton() noexcept { return pageSolo; }

private:
    void setupControls (const Config& cfg);

    bool melodic = true;

    juce::Label    styleLabel;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;

    juce::Label    rootLabel;
    juce::ComboBox rootBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttach;

    juce::Label    scaleLabel;
    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;

    juce::Label    octaveLabel;
    juce::ComboBox octaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octaveAttach;

    juce::TextButton pageMute { "M" };
    juce::TextButton pageSolo { "S" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentControlBar)
};
