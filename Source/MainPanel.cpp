#include "MainPanel.h"
#include "LeaderStylePresets.h"
#include "bootsy/BootsyStylePresets.h"
#include "stevie/StevieStylePresets.h"
#include "paul/PaulStylePresets.h"
#include "animal/AnimalStylePresets.h"

namespace
{
static void getBassMelodicMidiRange (const BassEngine& engine, int& minMidi, int& maxMidi)
{
    minMidi = 127;
    maxMidi = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < BootsyPreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        minMidi = juce::jmin (minMidi, h.midiNote);
        maxMidi = juce::jmax (maxMidi, h.midiNote);
    }
    if (minMidi > maxMidi)
    {
        const int r = engine.degreeToMidiNote (0, -1);
        minMidi = juce::jlimit (0, 127, r - 14);
        maxMidi = juce::jlimit (0, 127, r + 14);
    }
    else
    {
        minMidi = juce::jmax (0, minMidi - 2);
        maxMidi = juce::jmin (127, maxMidi + 2);
    }
}

static void getPianoMelodicMidiRange (const PianoEngine& engine, int& minMidi, int& maxMidi)
{
    minMidi = 127;
    maxMidi = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < SteviePreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        minMidi = juce::jmin (minMidi, h.midiNote);
        maxMidi = juce::jmax (maxMidi, h.midiNote);
    }
    if (minMidi > maxMidi)
    {
        const int r = engine.degreeToMidiNote (0, -1);
        minMidi = juce::jlimit (0, 127, r - 14);
        maxMidi = juce::jlimit (0, 127, r + 14);
    }
    else
    {
        minMidi = juce::jmax (0, minMidi - 2);
        maxMidi = juce::jmin (127, maxMidi + 2);
    }
}

static void getGuitarMelodicMidiRange (const GuitarEngine& engine, int& minMidi, int& maxMidi)
{
    minMidi = 127;
    maxMidi = 0;
    const auto& pat = engine.getPatternForGrid();
    for (int s = 0; s < PaulPreset::NUM_STEPS; ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active) continue;
        minMidi = juce::jmin (minMidi, h.midiNote);
        maxMidi = juce::jmax (maxMidi, h.midiNote);
    }
    if (minMidi > maxMidi)
    {
        const int r = engine.degreeToMidiNote (0, -1);
        minMidi = juce::jlimit (0, 127, r - 14);
        maxMidi = juce::jlimit (0, 127, r + 14);
    }
    else
    {
        minMidi = juce::jmax (0, minMidi - 2);
        maxMidi = juce::jmin (127, maxMidi + 2);
    }
}
}

MainPanel::LeaderKnob::LeaderKnob (const juce::String& paramId, const juce::String& name,
                                  juce::AudioProcessorValueTreeState& apvts)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffb0a8c4));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1c24));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8a8299));
    label.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void MainPanel::LeaderKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (14));
    slider.setBounds (b);
}

void MainPanel::StripPreview::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff252030));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour (0xff3a3548));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    switch (kind)
    {
        case Kind::animal:
        {
            const auto& pat = proc.drumEngine.getPatternForGrid();
            const float cw = bounds.getWidth() / (float) NUM_STEPS;
            const float ch = bounds.getHeight() / (float) NUM_DRUMS;
            for (int drum = 0; drum < NUM_DRUMS; ++drum)
            {
                for (int step = 0; step < NUM_STEPS; ++step)
                {
                    const auto& hit = pat[(size_t) step][(size_t) drum];
                    if (! hit.active) continue;
                    float a = juce::jlimit (0.15f, 1.f, (float) hit.velocity / 127.f);
                    g.setColour (juce::Colour (0xffe07a5a).withAlpha (a));
                    g.fillRect (bounds.getX() + (float) step * cw,
                                bounds.getBottom() - (float) (drum + 1) * ch,
                                juce::jmax (1.0f, cw - 0.5f),
                                juce::jmax (1.0f, ch - 0.5f));
                }
            }
            break;
        }
        case Kind::bootsy:
        {
            const auto& pat = proc.bassEngine.getPatternForGrid();
            int lo = 60, hi = 72;
            getBassMelodicMidiRange (proc.bassEngine, lo, hi);
            const int nRows = juce::jmax (1, hi - lo + 1);
            const float rowH = bounds.getHeight() / (float) nRows;
            const float cw = bounds.getWidth() / (float) BootsyPreset::NUM_STEPS;
            for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = bounds.getY() + (float) row * rowH;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xff5cb8a8).withAlpha (a));
                g.fillEllipse (bounds.getX() + (float) step * cw + 1.f, y + 1.f,
                               juce::jmax (2.f, cw - 2.f), juce::jmax (2.f, rowH - 2.f));
            }
            break;
        }
        case Kind::stevie:
        {
            const auto& pat = proc.pianoEngine.getPatternForGrid();
            int lo = 60, hi = 72;
            getPianoMelodicMidiRange (proc.pianoEngine, lo, hi);
            const int nRows = juce::jmax (1, hi - lo + 1);
            const float rowH = bounds.getHeight() / (float) nRows;
            const float cw = bounds.getWidth() / (float) SteviePreset::NUM_STEPS;
            for (int step = 0; step < SteviePreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = bounds.getY() + (float) row * rowH;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xffb88cff).withAlpha (a));
                g.fillEllipse (bounds.getX() + (float) step * cw + 1.f, y + 1.f,
                               juce::jmax (2.f, cw - 2.f), juce::jmax (2.f, rowH - 2.f));
            }
            break;
        }
        case Kind::paul:
        {
            const auto& pat = proc.guitarEngine.getPatternForGrid();
            int lo = 60, hi = 72;
            getGuitarMelodicMidiRange (proc.guitarEngine, lo, hi);
            const int nRows = juce::jmax (1, hi - lo + 1);
            const float rowH = bounds.getHeight() / (float) nRows;
            const float cw = bounds.getWidth() / (float) PaulPreset::NUM_STEPS;
            for (int step = 0; step < PaulPreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = bounds.getY() + (float) row * rowH;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xff6eb3ff).withAlpha (a));
                g.fillEllipse (bounds.getX() + (float) step * cw + 1.f, y + 1.f,
                               juce::jmax (2.f, cw - 2.f), juce::jmax (2.f, rowH - 2.f));
            }
            break;
        }
    }
}

void MainPanel::setupRow (Row& row,
                          const juce::String& rowName,
                          StripPreview::Kind previewKind,
                          const char* muteId,
                          const char* soloId)
{
    row.name.setText (rowName, juce::dontSendNotification);
    row.name.setFont (juce::Font (juce::FontOptions().withHeight (14.0f).withStyle ("Semibold")));
    row.name.setColour (juce::Label::textColourId, juce::Colours::white);
    row.name.setJustificationType (juce::Justification::centredLeft);

    row.preview = std::make_unique<StripPreview> (proc, previewKind);

    row.mute.setClickingTogglesState (true);
    row.solo.setClickingTogglesState (true);
    row.mute.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    row.solo.setMouseCursor (juce::MouseCursor::PointingHandCursor);

    row.muteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, muteId, row.mute);
    row.soloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, soloId, row.solo);

    mixerArea.addAndMakeVisible (row.name);
    mixerArea.addAndMakeVisible (*row.preview);
    mixerArea.addAndMakeVisible (row.mute);
    mixerArea.addAndMakeVisible (row.solo);
}

MainPanel::MainPanel (BridgeProcessor& p)
    : proc (p)
{
    title.setText ("Leader", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions().withHeight (22.0f).withStyle ("Semibold")));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (title);
    addAndMakeVisible (bandControls);
    addAndMakeVisible (mixerArea);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    styleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a8299));
    styleLabel.setJustificationType (juce::Justification::centredLeft);
    bandControls.addAndMakeVisible (styleLabel);

    for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        styleBox.addItem (LEADER_STYLE_NAMES[i], i + 1);
    styleBox.setTooltip ("Arrangement preset: nudges all five leader controls.");
    bandControls.addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (proc.apvtsMain, "leaderStyle", styleBox);

    knobPresence = std::make_unique<LeaderKnob> ("leaderPresence", "Presence", proc.apvtsMain);
    knobTight    = std::make_unique<LeaderKnob> ("leaderTight",    "Tight",    proc.apvtsMain);
    knobUnity    = std::make_unique<LeaderKnob> ("leaderUnity",    "Unity",    proc.apvtsMain);
    knobBreath   = std::make_unique<LeaderKnob> ("leaderBreath",   "Breath",   proc.apvtsMain);
    knobSpark    = std::make_unique<LeaderKnob> ("leaderSpark",    "Spark",    proc.apvtsMain);

    bandControls.addAndMakeVisible (*knobPresence);
    bandControls.addAndMakeVisible (*knobTight);
    bandControls.addAndMakeVisible (*knobUnity);
    bandControls.addAndMakeVisible (*knobBreath);
    bandControls.addAndMakeVisible (*knobSpark);

    setupRow (animal, "Animal", StripPreview::Kind::animal, "mainMuteAnimal", "mainSoloAnimal");
    setupRow (bootsy, "Bootsy", StripPreview::Kind::bootsy, "mainMuteBootsy", "mainSoloBootsy");
    setupRow (stevie, "Stevie", StripPreview::Kind::stevie, "mainMuteStevie", "mainSoloStevie");
    setupRow (paul, "Paul", StripPreview::Kind::paul, "mainMutePaul", "mainSoloPaul");

    proc.apvtsMain.state.addListener (this);
    applyLeaderEngaged();
    syncSoloButtonColours();
    startTimerHz (15);
}

MainPanel::~MainPanel()
{
    proc.apvtsMain.state.removeListener (this);
}

void MainPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    applyLeaderEngaged();
}

void MainPanel::syncSoloButtonColours()
{
    auto soloLit = [this] (const char* id) -> bool
    {
        if (auto* v = proc.apvtsMain.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };

    auto styleSolo = [] (juce::TextButton& b, bool on)
    {
        if (on)
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a3d28));
            b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff6ee7a0));
        }
        else
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffa8a8b8));
        }
    };

    auto styleMute = [] (juce::TextButton& b, bool muted)
    {
        if (muted)
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a2020));
            b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff9999));
        }
        else
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffa8a8b8));
        }
    };

    styleSolo (animal.solo, soloLit ("mainSoloAnimal"));
    styleSolo (bootsy.solo, soloLit ("mainSoloBootsy"));
    styleSolo (stevie.solo, soloLit ("mainSoloStevie"));
    styleSolo (paul.solo, soloLit ("mainSoloPaul"));

    auto muteLit = [this] (const char* id) -> bool
    {
        if (auto* v = proc.apvtsMain.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };

    styleMute (animal.mute, muteLit ("mainMuteAnimal"));
    styleMute (bootsy.mute, muteLit ("mainMuteBootsy"));
    styleMute (stevie.mute, muteLit ("mainMuteStevie"));
    styleMute (paul.mute, muteLit ("mainMutePaul"));
}

void MainPanel::applyLeaderEngaged()
{
    const bool on = proc.apvtsMain.getRawParameterValue ("leaderTabOn") != nullptr
                        && proc.apvtsMain.getRawParameterValue ("leaderTabOn")->load() > 0.5f;
    bandControls.setEnabled (on);
    bandControls.setAlpha (on ? 1.0f : 0.42f);
    mixerArea.setEnabled (on);
    mixerArea.setAlpha (on ? 1.0f : 0.42f);
}

void MainPanel::timerCallback()
{
    syncSoloButtonColours();
    if (animal.preview != nullptr) animal.preview->repaint();
    if (bootsy.preview != nullptr) bootsy.preview->repaint();
    if (stevie.preview != nullptr) stevie.preview->repaint();
    if (paul.preview != nullptr) paul.preview->repaint();
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14121a));
}

void MainPanel::resized()
{
    auto r = getLocalBounds().reduced (20);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto bandR = r.removeFromTop (198);
    bandControls.setBounds (bandR);
    {
        auto b = bandControls.getLocalBounds();
        auto styleRow = b.removeFromTop (30);
        styleLabel.setBounds (styleRow.removeFromLeft (44));
        styleBox.setBounds (styleRow.removeFromLeft (juce::jmin (320, styleRow.getWidth())).reduced (0, 2));

        b.removeFromTop (8);
        const int knobH = 86;
        auto row1 = b.removeFromTop (knobH);
        const int gap = 6;
        const int n1 = 3;
        int kw = (row1.getWidth() - gap * (n1 - 1)) / n1;
        knobPresence->setBounds (row1.removeFromLeft (kw));
        row1.removeFromLeft (gap);
        knobTight->setBounds (row1.removeFromLeft (kw));
        row1.removeFromLeft (gap);
        knobUnity->setBounds (row1.removeFromLeft (kw));

        b.removeFromTop (4);
        auto row2 = b.removeFromTop (knobH);
        const int n2 = 2;
        int kw2 = (row2.getWidth() - gap) / n2;
        knobBreath->setBounds (row2.removeFromLeft (kw2));
        row2.removeFromLeft (gap);
        knobSpark->setBounds (row2.removeFromLeft (kw2));
    }

    r.removeFromTop (10);
    mixerArea.setBounds (r);

    auto area = mixerArea.getLocalBounds();
    const int rowH = 56;
    const int gap = 8;
    const int nameW = 52;
    const int prevW = juce::jmax (160, area.getWidth() - nameW - 80);
    const int msW = 28;

    auto place = [&] (Row& row)
    {
        auto rowB = area.removeFromTop (rowH);
        area.removeFromTop (gap);
        auto rb = rowB;
        row.name.setBounds (rb.removeFromLeft (nameW));
        row.preview->setBounds (rb.removeFromLeft (prevW).reduced (0, 4));
        rb.removeFromLeft (6);
        row.mute.setBounds (rb.removeFromLeft (msW).withHeight (22).translated (0, 10));
        row.solo.setBounds (rb.removeFromLeft (msW).withHeight (22).translated (0, 10));
    };

    place (animal);
    place (bootsy);
    place (stevie);
    place (paul);
}
