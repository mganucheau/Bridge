#include "BridgeSettingsDialog.h"

BridgeSettingsDialog::BridgeSettingsDialog()
{
    addAndMakeVisible (title);
    title.setFont (juce::Font (juce::FontOptions().withHeight (20.0f).withStyle ("Semibold")));
    title.setColour (juce::Label::textColourId, juce::Colour (0xffeae2d5));

    for (auto* l : { &midiInLabel, &midiOutLabel })
    {
        l->setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
        l->setColour (juce::Label::textColourId, juce::Colour (0xffcac4d0));
        addAndMakeVisible (*l);
    }

    midiInBox.addItem ("(Host default)", 1);
    midiOutBox.addItem ("(Host default)", 1);
    midiInBox.setSelectedId (1);
    midiOutBox.setSelectedId (1);
    addAndMakeVisible (midiInBox);
    addAndMakeVisible (midiOutBox);

    aboutLabel.setText ("About", juce::dontSendNotification);
    aboutLabel.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    aboutLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcac4d0));
    addAndMakeVisible (aboutLabel);

    aboutBody.setMultiLine (true);
    aboutBody.setReadOnly (true);
    aboutBody.setScrollbarsShown (true);
    aboutBody.setCaretVisible (false);
    aboutBody.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c1b20));
    aboutBody.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe8e4ec));
    aboutBody.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    aboutBody.setText ("Bridge — multi-engine MIDI ensemble\n"
                       "Version 1.0.0\n"
                       "© 2026 Bridge Audio. All rights reserved.\n\n"
                       "This plugin generates MIDI for drums, bass, keys, and guitar. "
                       "MIDI routing follows the host’s track and device assignments.\n\n"
                       "For support and updates, see the product page linked from your installer.");
    addAndMakeVisible (aboutBody);
}

void BridgeSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14121a));
}

void BridgeSettingsDialog::resized()
{
    auto r = getLocalBounds().reduced (20);
    title.setBounds (r.removeFromTop (32));
    r.removeFromTop (16);

    auto midiRow = r.removeFromTop (28);
    midiInLabel.setBounds (midiRow.removeFromLeft (100));
    midiInBox.setBounds (midiRow.removeFromLeft (220).reduced (0, 2));
    r.removeFromTop (10);

    midiRow = r.removeFromTop (28);
    midiOutLabel.setBounds (midiRow.removeFromLeft (100));
    midiOutBox.setBounds (midiRow.removeFromLeft (220).reduced (0, 2));
    r.removeFromTop (20);

    aboutLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    aboutBody.setBounds (r);
}
