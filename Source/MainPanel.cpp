#include "MainPanel.h"

MainPanel::MainPanel (BridgeProcessor& p)
    : proc (p)
{
    subtitle.setText ("Enable generators for MIDI output. Edit each on its own tab.", juce::dontSendNotification);
    subtitle.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    subtitle.setColour (juce::Label::textColourId, juce::Colour (0xffb0a8c4));
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);

    auto setupRow = [this] (Row& row, const juce::String& paramId, const juce::String& name, const juce::String& hint)
    {
        row.name.setText (name, juce::dontSendNotification);
        row.name.setFont (juce::Font (juce::FontOptions().withHeight (15.0f).withStyle ("Semibold")));
        row.name.setColour (juce::Label::textColourId, juce::Colours::white);
        row.hint.setText (hint, juce::dontSendNotification);
        row.hint.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        row.hint.setColour (juce::Label::textColourId, juce::Colour (0xff8a8299));
        row.toggle.setButtonText ("on");
        row.toggle.setClickingTogglesState (true);
        row.attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (proc.apvtsMain, paramId, row.toggle);
        addAndMakeVisible (row.name);
        addAndMakeVisible (row.hint);
        addAndMakeVisible (row.toggle);
    };

    title.setFont (juce::Font (juce::FontOptions().withHeight (22.0f).withStyle ("Semibold")));
    title.setColour (juce::Label::textColourId, juce::Colours::white);

    setupRow (animalRow,  "animalOn",  "Animal",  "Generative drums (MIDI ch 10)");
    setupRow (bootsyRow,  "bootsyOn",  "Bootsy",  "Generative bass line");
    setupRow (stevieRow,  "stevieOn",  "Stevie",  "Generative piano / keys");
    setupRow (paulRow,    "paulOn",    "Paul",    "Generative guitar line");
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14121a));
}

void MainPanel::resized()
{
    auto r = getLocalBounds().reduced (24);
    title.setBounds (r.removeFromTop (32));
    r.removeFromTop (6);
    subtitle.setBounds (r.removeFromTop (40));
    r.removeFromTop (16);

    const int rowH = 72;
    const int gap = 8;

    auto placeRow = [&r, rowH, gap] (Row& row)
    {
        auto rowBounds = r.removeFromTop (rowH);
        r.removeFromTop (gap);
        auto right = rowBounds.removeFromRight (100);
        row.toggle.setBounds (right.reduced (0, 12));
        row.name.setBounds (rowBounds.removeFromTop (22));
        row.hint.setBounds (rowBounds.removeFromTop (36));
    };

    placeRow (animalRow);
    placeRow (bootsyRow);
    placeRow (stevieRow);
    placeRow (paulRow);
}
