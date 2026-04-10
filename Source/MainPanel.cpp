#include "MainPanel.h"
#include "LeaderStylePresets.h"
#include "MelodicGridLayout.h"
#include "bootsy/BootsyStylePresets.h"
#include "stevie/StevieStylePresets.h"
#include "paul/PaulStylePresets.h"
#include "animal/AnimalStylePresets.h"

void MainPanel::StripPreview::paint (juce::Graphics& g)
{
    using namespace AnimalM3;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (surfaceContainerHigh);
    g.fillRoundedRectangle (bounds, cornerExtraSmall);
    g.setColour (outline.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerExtraSmall, 1.0f);

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
            bridge::setOneOctaveMelodicRange (proc.bassEngine, lo, hi);
            const int nRows = bridge::kMelodicOctaveRows;
            float ox = 0.0f, oy = 0.0f, cs = 1.0f;
            bridge::computeSquareMelodicGrid (bounds.getX(), bounds.getWidth(), bounds.getY(), bounds.getHeight(),
                                              BootsyPreset::NUM_STEPS, nRows, ox, oy, cs);
            for (int step = 0; step < BootsyPreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = oy + (float) row * cs;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xff5cb8a8).withAlpha (a));
                g.fillEllipse (ox + (float) step * cs + 1.f, y + 1.f,
                               juce::jmax (2.f, cs - 2.f), juce::jmax (2.f, cs - 2.f));
            }
            break;
        }
        case Kind::stevie:
        {
            const auto& pat = proc.pianoEngine.getPatternForGrid();
            int lo = 60, hi = 72;
            bridge::setOneOctaveMelodicRange (proc.pianoEngine, lo, hi);
            const int nRows = bridge::kMelodicOctaveRows;
            float ox = 0.0f, oy = 0.0f, cs = 1.0f;
            bridge::computeSquareMelodicGrid (bounds.getX(), bounds.getWidth(), bounds.getY(), bounds.getHeight(),
                                              SteviePreset::NUM_STEPS, nRows, ox, oy, cs);
            for (int step = 0; step < SteviePreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = oy + (float) row * cs;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xffb88cff).withAlpha (a));
                g.fillEllipse (ox + (float) step * cs + 1.f, y + 1.f,
                               juce::jmax (2.f, cs - 2.f), juce::jmax (2.f, cs - 2.f));
            }
            break;
        }
        case Kind::paul:
        {
            const auto& pat = proc.guitarEngine.getPatternForGrid();
            int lo = 60, hi = 72;
            bridge::setOneOctaveMelodicRange (proc.guitarEngine, lo, hi);
            const int nRows = bridge::kMelodicOctaveRows;
            float ox = 0.0f, oy = 0.0f, cs = 1.0f;
            bridge::computeSquareMelodicGrid (bounds.getX(), bounds.getWidth(), bounds.getY(), bounds.getHeight(),
                                              PaulPreset::NUM_STEPS, nRows, ox, oy, cs);
            for (int step = 0; step < PaulPreset::NUM_STEPS; ++step)
            {
                const auto& h = pat[(size_t) step];
                if (! h.active) continue;
                int row = juce::jlimit (0, nRows - 1, hi - h.midiNote);
                float y = oy + (float) row * cs;
                float a = juce::jlimit (0.2f, 1.f, (float) h.velocity / 127.f);
                g.setColour (juce::Colour (0xff6eb3ff).withAlpha (a));
                g.fillEllipse (ox + (float) step * cs + 1.f, y + 1.f,
                               juce::jmax (2.f, cs - 2.f), juce::jmax (2.f, cs - 2.f));
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
    row.name.setColour (juce::Label::textColourId, AnimalColors::TextPrimary);
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
    : proc (p),
      knobPresence ("leaderPresence", "Presence", p.apvtsMain),
      knobTight ("leaderTight", "Tight", p.apvtsMain),
      knobUnity ("leaderUnity", "Unity", p.apvtsMain),
      knobBreath ("leaderBreath", "Breath", p.apvtsMain),
      knobSpark ("leaderSpark", "Spark", p.apvtsMain)
{
    setLookAndFeel (&laf);

    title.setText ("Leader", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions().withHeight (22.0f).withStyle ("Semibold")));
    title.setColour (juce::Label::textColourId, AnimalColors::TextPrimary);
    addAndMakeVisible (title);
    addAndMakeVisible (bandControls);
    addAndMakeVisible (mixerArea);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    styleLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    styleLabel.setJustificationType (juce::Justification::centredLeft);
    bandControls.addAndMakeVisible (styleLabel);

    for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        styleBox.addItem (LEADER_STYLE_NAMES[i], i + 1);
    styleBox.setTooltip ("Arrangement preset: nudges all five leader controls.");
    bandControls.addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (proc.apvtsMain, "leaderStyle", styleBox);

    bandControls.addAndMakeVisible (knobPresence);
    bandControls.addAndMakeVisible (knobTight);
    bandControls.addAndMakeVisible (knobUnity);
    bandControls.addAndMakeVisible (knobBreath);
    bandControls.addAndMakeVisible (knobSpark);

    setupRow (animal, "Drums", StripPreview::Kind::animal, "mainMuteAnimal", "mainSoloAnimal");
    setupRow (bootsy, "Bass", StripPreview::Kind::bootsy, "mainMuteBootsy", "mainSoloBootsy");
    setupRow (stevie, "Keys", StripPreview::Kind::stevie, "mainMuteStevie", "mainSoloStevie");
    setupRow (paul, "Guitar", StripPreview::Kind::paul, "mainMutePaul", "mainSoloPaul");

    proc.apvtsMain.state.addListener (this);
    applyLeaderEngaged();
    syncSoloButtonColours();
    startTimerHz (15);
}

MainPanel::~MainPanel()
{
    proc.apvtsMain.state.removeListener (this);
    setLookAndFeel (nullptr);
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
    using namespace AnimalM3;
    g.fillAll (surfaceDim);

    auto drawCard = [&] (juce::Component& c)
    {
        auto rf = c.getBounds().toFloat().reduced (0.5f);
        drawShadow (g, rf, cornerLarge, 1);
        fillSurface (g, rf, surfaceContainer, cornerLarge);
        g.setColour (outline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), cornerLarge, 1.0f);
    };

    drawCard (mixerArea);
    drawCard (bandControls);
}

void MainPanel::resized()
{
    constexpr int edge = 24;
    constexpr int bandH = 212;
    auto r = getLocalBounds().reduced (edge);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (12);

    auto bandR = r.removeFromBottom (bandH);
    r.removeFromBottom (12);
    mixerArea.setBounds (r);
    bandControls.setBounds (bandR);
    {
        auto b = bandControls.getLocalBounds().reduced (16, 14);
        auto styleRow = b.removeFromTop (30);
        styleLabel.setBounds (styleRow.removeFromLeft (48));
        styleBox.setBounds (styleRow.removeFromLeft (juce::jmin (320, styleRow.getWidth())).reduced (0, 2));

        b.removeFromTop (12);
        const int knobH = 88;
        auto row1 = b.removeFromTop (knobH);
        const int gap = 8;
        const int n1 = 3;
        int kw = (row1.getWidth() - gap * (n1 - 1)) / n1;
        knobPresence.setBounds (row1.removeFromLeft (kw));
        row1.removeFromLeft (gap);
        knobTight.setBounds (row1.removeFromLeft (kw));
        row1.removeFromLeft (gap);
        knobUnity.setBounds (row1.removeFromLeft (kw));

        b.removeFromTop (6);
        auto row2 = b.removeFromTop (knobH);
        const int n2 = 2;
        int kw2 = (row2.getWidth() - gap) / n2;
        knobBreath.setBounds (row2.removeFromLeft (kw2));
        row2.removeFromLeft (gap);
        knobSpark.setBounds (row2.removeFromLeft (kw2));
    }

    auto area = mixerArea.getLocalBounds();
    const int rowH = 56;
    const int gap = 8;
    const int nameW = 64;
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
