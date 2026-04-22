#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

#include "BridgeEditorChromeLookAndFeel.h"
#include "BridgeInstrumentTabRail.h"

class BridgeProcessor;

/**
 * 54px header: branding (left), centered transport, instrument tabs + settings (right).
 * Spec: #1E1E1E bar, Lucide-style layout (implemented in JUCE; no React).
 */
class BridgeHeaderBar : public juce::Component
{
public:
    explicit BridgeHeaderBar (BridgeProcessor& processor,
                              std::function<void (int tabIndex)> onSelectInstrumentTab);
    ~BridgeHeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void syncInstrumentTabSelection (int activeTabIndex);
    void syncBpmDisplay();
    bool bpmValueEditorHasKeyboardFocus() const;

    BridgeProcessor& proc;

private:
    struct GoldBar final : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xffd4af37)); }
    };

    BridgeEditorChromeLookAndFeel chromeLaf;

    GoldBar          goldBar;
    juce::Label      logoLabel;
    juce::Label      bpmValueLabel;
    juce::Label      bpmUnitLabel;
    juce::ShapeButton playButton  { "Play",  juce::Colours::white, juce::Colours::white, juce::Colours::white };
    juce::ShapeButton stopButton  { "Stop",  juce::Colours::white, juce::Colours::white, juce::Colours::white };
    juce::ComboBox   timeDivisionBox;
    juce::TextButton hostSyncButton { "Host sync" };
    juce::ShapeButton gearButton { "Settings", juce::Colours::transparentBlack, juce::Colours::transparentBlack,
                                   juce::Colours::transparentBlack };

    std::unique_ptr<BridgeInstrumentTabRail> tabRail;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>     transportAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   timeDivisionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>     hostSyncAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeHeaderBar)
};

namespace bridge::headerSpec
{
inline constexpr int   kHeaderHeight   = 54;
inline constexpr int   kGoldBarW       = 4;
inline constexpr int   kGoldBarH       = 22;
inline constexpr int   kGapAfterGold   = 8;
inline constexpr int   kPadX           = 16;
inline constexpr int   kTransportGap   = 12;
inline constexpr int   kCtrlRowH       = 32;
inline constexpr int   kTabToSettingsGap = 12;
inline constexpr int   kGearSide       = 18;
inline constexpr int   kTransportBtnSide = 16;
inline constexpr int   kBpmFieldW      = 64;
inline constexpr int   kBpmLabelW      = 36;
inline constexpr int   kDivComboW      = 128;
inline constexpr auto  kBarBg          = juce::uint32 (0xff1e1e1e);
} // namespace bridge::headerSpec
