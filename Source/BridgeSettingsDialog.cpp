#include "BridgeSettingsDialog.h"
#include "BridgeProcessor.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"

BridgeSettingsDialog::BridgeSettingsDialog (BridgeProcessor& processor)
    : proc (processor)
{
    addAndMakeVisible (title);
    title.setFont (bridge::hig::uiFont (20.0f, "Semibold"));
    title.setColour (juce::Label::textColourId, bridge::colors::textPrimary());

    addAndMakeVisible (themeLabel);
    themeLabel.setFont (bridge::hig::uiFont (13.0f));
    themeLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("uiTheme")))
    {
        const auto& names = p->choices;
        for (int i = 0; i < names.size(); ++i)
            themeBox.addItem (names[i], i + 1);
    }
    addAndMakeVisible (themeBox);
    themeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvtsMain, "uiTheme", themeBox);

    addAndMakeVisible (midiRoutingLabel);
    midiRoutingLabel.setFont (bridge::hig::uiFont (13.0f));
    midiRoutingLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());

    midiRoutingBody.setMultiLine (true);
    midiRoutingBody.setReadOnly (true);
    midiRoutingBody.setScrollbarsShown (true);
    midiRoutingBody.setCaretVisible (false);
    midiRoutingBody.setColour (juce::TextEditor::backgroundColourId, bridge::colors::cardSurface());
    midiRoutingBody.setColour (juce::TextEditor::textColourId, bridge::colors::textPrimary());
    midiRoutingBody.setFont (bridge::hig::uiFont (12.0f));
    {
        juce::String s = "The plugin exposes one merged MIDI stream on the main output bus in this build, plus named auxiliary output buses (Leader / Drums / Bass / Keys / Guitar) for hosts that list disabled-audio MIDI buses.\n\n";
        s << "Per-instrument MIDI channel is still set on each instrument page. If your DAW shows only one MIDI output, route by channel or use the plugin’s multiple tracks workflow.";
        midiRoutingBody.setText (s);
    }
    addAndMakeVisible (midiRoutingBody);

    aboutLabel.setText ("About", juce::dontSendNotification);
    aboutLabel.setFont (bridge::hig::uiFont (13.0f));
    aboutLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
    addAndMakeVisible (aboutLabel);

    aboutBody.setMultiLine (true);
    aboutBody.setReadOnly (true);
    aboutBody.setScrollbarsShown (true);
    aboutBody.setCaretVisible (false);
    aboutBody.setColour (juce::TextEditor::backgroundColourId, bridge::colors::cardSurface());
    aboutBody.setColour (juce::TextEditor::textColourId, bridge::colors::textPrimary());
    aboutBody.setFont (bridge::hig::uiFont (13.0f));
    aboutBody.setText ("Bridge — multi-engine MIDI ensemble\n"
                       "Version 1.0.0\n"
                       "© 2026 Bridge Audio. All rights reserved.\n\n"
                       "This plugin generates MIDI for drums, bass, keys, and guitar. "
                       "Use the theme control above to switch curated light/dark palettes.");
    addAndMakeVisible (aboutBody);
}

void BridgeSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (bridge::colors::background());
}

void BridgeSettingsDialog::resized()
{
    auto r = getLocalBounds().reduced (20);
    title.setBounds (r.removeFromTop (32));
    r.removeFromTop (16);

    auto themeRow = r.removeFromTop (28);
    themeLabel.setBounds (themeRow.removeFromLeft (100));
    themeBox.setBounds (themeRow.removeFromLeft (320).reduced (0, 2));
    r.removeFromTop (14);

    midiRoutingLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    midiRoutingBody.setBounds (r.removeFromTop (100));
    r.removeFromTop (16);

    aboutLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    aboutBody.setBounds (r);
}
