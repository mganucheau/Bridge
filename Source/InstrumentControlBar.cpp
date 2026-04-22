#include "InstrumentControlBar.h"
#include "BridgeProcessor.h"
#include "BridgeInstrumentStripStyle.h"
#include "BridgeInstrumentStyles.h"
#include "BridgePanelLayout.h"
#include "bass/BassStylePresets.h"
#include "piano/PianoStylePresets.h"
#include "guitar/GuitarStylePresets.h"

namespace
{
static void setupStripMute (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setConnectedEdges (0);
    bridge::instrumentStripStyle::tagStripMute (b);
    b.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

static void setupStripSolo (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setConnectedEdges (0);
    bridge::instrumentStripStyle::tagStripSolo (b);
    b.setColour (juce::TextButton::buttonColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::buttonOnColourId, bridge::instrumentStripStyle::fieldBg());
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}
} // namespace

InstrumentControlBar::Config InstrumentControlBar::makeLeaderConfig (BridgeProcessor& p)
{
    Config c;
    c.melodic = true;
    c.styleApvts = &p.apvtsMain;
    c.styleParamId = "leaderStyle";
    c.tonalApvts = &p.apvtsMain;
    c.rootParamId = "rootNote";
    c.scaleParamId = "scale";
    c.octaveParamId = "octave";
    c.numScales = BassPreset::NUM_SCALES;
    c.scaleNames = BassPreset::SCALE_NAMES;
    c.soloApvts = &p.apvtsMain;
    c.soloParamId = {};
    return c;
}

InstrumentControlBar::Config InstrumentControlBar::makeDrumsConfig (BridgeProcessor& p)
{
    Config c;
    c.melodic = false;
    c.styleApvts = &p.apvtsDrums;
    c.styleParamId = "style";
    c.tonalApvts = nullptr;
    c.soloApvts = &p.apvtsMain;
    c.soloParamId = "mainSoloDrums";
    return c;
}

InstrumentControlBar::Config InstrumentControlBar::makeBassConfig (BridgeProcessor& p)
{
    Config c;
    c.melodic = true;
    c.styleApvts = &p.apvtsBass;
    c.styleParamId = "style";
    c.tonalApvts = &p.apvtsBass;
    c.rootParamId = "rootNote";
    c.scaleParamId = "scale";
    c.octaveParamId = "octave";
    c.numScales = BassPreset::NUM_SCALES;
    c.scaleNames = BassPreset::SCALE_NAMES;
    c.soloApvts = &p.apvtsMain;
    c.soloParamId = "mainSoloBass";
    return c;
}

InstrumentControlBar::Config InstrumentControlBar::makePianoConfig (BridgeProcessor& p)
{
    Config c;
    c.melodic = true;
    c.styleApvts = &p.apvtsPiano;
    c.styleParamId = "style";
    c.tonalApvts = &p.apvtsPiano;
    c.rootParamId = "rootNote";
    c.scaleParamId = "scale";
    c.octaveParamId = "octave";
    c.numScales = PianoPreset::NUM_SCALES;
    c.scaleNames = PianoPreset::SCALE_NAMES;
    c.soloApvts = &p.apvtsMain;
    c.soloParamId = "mainSoloPiano";
    return c;
}

InstrumentControlBar::Config InstrumentControlBar::makeGuitarConfig (BridgeProcessor& p)
{
    Config c;
    c.melodic = true;
    c.styleApvts = &p.apvtsGuitar;
    c.styleParamId = "style";
    c.tonalApvts = &p.apvtsGuitar;
    c.rootParamId = "rootNote";
    c.scaleParamId = "scale";
    c.octaveParamId = "octave";
    c.numScales = GuitarPreset::NUM_SCALES;
    c.scaleNames = GuitarPreset::SCALE_NAMES;
    c.soloApvts = &p.apvtsMain;
    c.soloParamId = "mainSoloGuitar";
    return c;
}

InstrumentControlBar::InstrumentControlBar (const Config& cfg)
    : melodic (cfg.melodic)
{
    setupControls (cfg);
}

void InstrumentControlBar::setupControls (const Config& cfg)
{
    bridge::instrumentStripStyle::applyStripLabel (styleLabel, "Style");
    bridge::instrumentStripStyle::applyStripComboColours (styleBox);
    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);

    addAndMakeVisible (styleLabel);
    addAndMakeVisible (styleBox);

    if (cfg.styleApvts != nullptr && cfg.styleParamId.isNotEmpty())
        if (cfg.styleApvts->getParameter (cfg.styleParamId))
            styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                *cfg.styleApvts, cfg.styleParamId, styleBox);

    if (melodic && cfg.tonalApvts != nullptr)
    {
        bridge::instrumentStripStyle::applyStripLabel (rootLabel, "Root");
        bridge::instrumentStripStyle::applyStripLabel (scaleLabel, "Scale");
        bridge::instrumentStripStyle::applyStripLabel (octaveLabel, "Oct");
        bridge::instrumentStripStyle::applyStripComboColours (rootBox);
        bridge::instrumentStripStyle::applyStripComboColours (scaleBox);
        bridge::instrumentStripStyle::applyStripComboColours (octaveBox);

        addAndMakeVisible (rootLabel);
        addAndMakeVisible (rootBox);
        addAndMakeVisible (scaleLabel);
        addAndMakeVisible (scaleBox);
        addAndMakeVisible (octaveLabel);
        addAndMakeVisible (octaveBox);

        static const char* roots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            rootBox.addItem (roots[i], i + 1);

        for (int i = 0; i < cfg.numScales; ++i)
            scaleBox.addItem (cfg.scaleNames[i], i + 1);

        for (int i = 1; i <= 4; ++i)
            octaveBox.addItem (juce::String (i), i);

        auto& tap = *cfg.tonalApvts;
        if (tap.getParameter (cfg.rootParamId))
            rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                tap, cfg.rootParamId, rootBox);
        if (tap.getParameter (cfg.scaleParamId))
            scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                tap, cfg.scaleParamId, scaleBox);
        if (tap.getParameter (cfg.octaveParamId))
            octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                tap, cfg.octaveParamId, octaveBox);
    }

    setupStripMute (pageMute);
    setupStripSolo (pageSolo);
    addAndMakeVisible (pageMute);
    addAndMakeVisible (pageSolo);

    if (cfg.soloApvts != nullptr && cfg.soloParamId.isNotEmpty())
        if (cfg.soloApvts->getParameter (cfg.soloParamId))
            soloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                *cfg.soloApvts, cfg.soloParamId, pageSolo);
}

void InstrumentControlBar::paint (juce::Graphics& g)
{
    g.fillAll (bridge::instrumentStripStyle::controlBarBackground());
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawLine (0.0f, h - 0.5f, w, h - 0.5f, 1.0f);
}

void InstrumentControlBar::resized()
{
    constexpr int kGapGroups = 30;
    constexpr int kLabelComboGap = 8;
    constexpr int kMsSide = 28;
    constexpr int kMsGap = 6;
    constexpr int kPadX = 16;

    const int comboH = bridge::panelLayout::kInstrumentComboH;
    const int comboY = (getHeight() - comboH) / 2;
    const int labelH = getHeight();
    const int msY = (getHeight() - kMsSide) / 2;

    auto row = getLocalBounds().reduced (kPadX, 0);

    const int msClusterW = kMsSide + kMsGap + kMsSide;
    auto msArea = row.removeFromRight (msClusterW);
    pageSolo.setBounds (msArea.removeFromRight (kMsSide).withY (msY).withHeight (kMsSide));
    msArea.removeFromRight (kMsGap);
    pageMute.setBounds (msArea.removeFromRight (kMsSide).withY (msY).withHeight (kMsSide));

    auto placeGroup = [&] (juce::Label& lab, int lw, juce::ComboBox& box, int bw)
    {
        lab.setBounds (row.removeFromLeft (lw).withHeight (labelH));
        row.removeFromLeft (kLabelComboGap);
        box.setBounds (row.removeFromLeft (bw).withY (comboY).withHeight (comboH));
    };

    placeGroup (styleLabel, 42, styleBox, 164);
    if (melodic)
    {
        row.removeFromLeft (kGapGroups);
        placeGroup (rootLabel, 40, rootBox, 52);
        row.removeFromLeft (kGapGroups);
        placeGroup (scaleLabel, 45, scaleBox, 160);
        row.removeFromLeft (kGapGroups);
        placeGroup (octaveLabel, 35, octaveBox, 52);
    }
}

void InstrumentControlBar::setTrackPowered (bool on)
{
    styleLabel.setEnabled (on);
    styleBox.setEnabled (on);
    if (melodic)
    {
        rootLabel.setEnabled (on);
        rootBox.setEnabled (on);
        scaleLabel.setEnabled (on);
        scaleBox.setEnabled (on);
        octaveLabel.setEnabled (on);
        octaveBox.setEnabled (on);
    }

    pageMute.setEnabled (true);
    pageMute.setToggleState (! on, juce::dontSendNotification);
    pageSolo.setEnabled (on);
    pageSolo.setAlpha (on ? 1.0f : 0.42f);

    const float dim = on ? 1.0f : 0.42f;
    styleLabel.setAlpha (dim);
    styleBox.setAlpha (dim);
    if (melodic)
    {
        rootLabel.setAlpha (dim);
        rootBox.setAlpha (dim);
        scaleLabel.setAlpha (dim);
        scaleBox.setAlpha (dim);
        octaveLabel.setAlpha (dim);
        octaveBox.setAlpha (dim);
    }
}
