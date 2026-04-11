#include "MainPanel.h"
#include "BridgeInstrumentStyles.h"
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

    auto setupSectionLabel = [] (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
        lab.setColour (juce::Label::textColourId, AnimalColors::TextDim);
        lab.setJustificationType (juce::Justification::centredLeft);
    };
    setupSectionLabel (mixLabel,     "MIX");
    setupSectionLabel (leaderLabel,  "LEADER");
    setupSectionLabel (actionsLabel, "ACTIONS");
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (leaderLabel);
    addAndMakeVisible (actionsLabel);

    addAndMakeVisible (mixerArea);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    styleLabel.setColour (juce::Label::textColourId, AnimalColors::TextDim);
    styleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        styleBox.addItem (LEADER_STYLE_NAMES[i], i + 1);
    styleBox.setTooltip ("Arrangement preset: nudges all five leader controls.");
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (proc.apvtsMain, "leaderStyle", styleBox);

    addAndMakeVisible (knobPresence);
    addAndMakeVisible (knobTight);
    addAndMakeVisible (knobUnity);
    addAndMakeVisible (knobBreath);
    addAndMakeVisible (knobSpark);

    // Loop-card contents: ENGAGE toggle (bound to leaderTabOn) + RESET button
    engageButton.setClickingTogglesState (true);
    engageButton.setTooltip ("Engage the Leader: when off, the mix knobs and arrangement preset are bypassed.");
    engageButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff23202b));
    engageButton.setColour (juce::TextButton::buttonOnColourId, AnimalColors::Accent.withAlpha (0.55f));
    engageButton.setColour (juce::TextButton::textColourOffId,  AnimalColors::TextDim);
    engageButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white.withAlpha (0.95f));
    engageAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (proc.apvtsMain, "leaderTabOn", engageButton);
    addAndMakeVisible (engageButton);

    resetButton.setTooltip ("Reset the Leader knobs and style to their defaults.");
    resetButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff23202b));
    resetButton.setColour (juce::TextButton::textColourOffId, AnimalColors::TextDim);
    resetButton.onClick = [this]
    {
        auto resetFloatToDefault = [this] (const juce::String& id)
        {
            if (auto* p = proc.apvtsMain.getParameter (id))
                p->setValueNotifyingHost (p->getDefaultValue());
        };
        resetFloatToDefault ("leaderPresence");
        resetFloatToDefault ("leaderTight");
        resetFloatToDefault ("leaderUnity");
        resetFloatToDefault ("leaderBreath");
        resetFloatToDefault ("leaderSpark");
        resetFloatToDefault ("leaderStyle");
    };
    addAndMakeVisible (resetButton);

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

    const float alpha = on ? 1.0f : 0.42f;

    knobPresence.setEnabled (on); knobPresence.setAlpha (alpha);
    knobTight.setEnabled    (on); knobTight.setAlpha    (alpha);
    knobUnity.setEnabled    (on); knobUnity.setAlpha    (alpha);
    knobBreath.setEnabled   (on); knobBreath.setAlpha   (alpha);
    knobSpark.setEnabled    (on); knobSpark.setAlpha    (alpha);

    styleLabel.setAlpha (alpha);
    styleBox.setEnabled (on);
    styleBox.setAlpha   (alpha);

    mixLabel.setAlpha (alpha);
    mixerArea.setEnabled (on);
    mixerArea.setAlpha   (alpha);
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
    using namespace bridge::instrumentLayout;
    g.fillAll (surface);

    auto full = getLocalBounds().reduced (kPanelEdge, kPanelEdge);

    // Main-area card (mixer) — painted behind mixerArea.
    full.removeFromTop (kDropdownRowH + kSectionGap);
    auto mainCard = full.removeFromTop (kMainAreaH);
    full.removeFromTop (kSectionGap);
    auto bottom = full.removeFromTop (kBottomCardH);
    auto loopCard  = bottom.removeFromRight (kLoopCardW);
    bottom.removeFromRight (kCardGap);
    auto knobsCard = bottom;

    auto drawCard = [&] (juce::Rectangle<int> r)
    {
        auto rf = r.toFloat();
        drawShadow (g, rf, (float) kCardRadius, 1);
        fillSurface (g, rf, surfaceContainer, (float) kCardRadius);
        g.setColour (outline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), (float) kCardRadius, 1.0f);
    };
    drawCard (mainCard);
    drawCard (knobsCard);
    drawCard (loopCard);
}

void MainPanel::resized()
{
    using namespace bridge::instrumentLayout;

    auto outer = getLocalBounds().reduced (kPanelEdge, kPanelEdge);

    // 1. Dropdown row — only the Style picker for the Leader tab.
    {
        auto row = outer.removeFromTop (kDropdownRowH);
        const int h = kDropdownH;
        const int labY = row.getCentreY() - (int) (h * 0.5f);
        styleLabel.setBounds (row.removeFromLeft (48).withHeight (h).withY (labY));
        row.removeFromLeft (4);
        styleBox.setBounds   (row.removeFromLeft (220).withHeight (h).withY (labY));
    }
    outer.removeFromTop (kSectionGap);

    // 2. Main area — mixer card with four rows.
    auto mainArea = outer.removeFromTop (kMainAreaH);
    mixerArea.setBounds (mainArea);
    outer.removeFromTop (kSectionGap);

    // 3. Bottom: knobs card (left) + loop/actions card (right).
    auto bottomArea = outer.removeFromTop (kBottomCardH);
    auto loopCard   = bottomArea.removeFromRight (kLoopCardW);
    bottomArea.removeFromRight (kCardGap);
    auto knobsCard  = bottomArea;

    // ── Knobs card: MIX header + row of 5 Leader knobs ────────────────────
    {
        auto inner = knobsCard.reduced (14, 12);
        mixLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        auto row = inner;
        const int n = 5;
        const int gap = 6;
        const int kw = (row.getWidth() - gap * (n - 1)) / n;
        knobPresence.setBounds (row.removeFromLeft (kw)); row.removeFromLeft (gap);
        knobTight.setBounds    (row.removeFromLeft (kw)); row.removeFromLeft (gap);
        knobUnity.setBounds    (row.removeFromLeft (kw)); row.removeFromLeft (gap);
        knobBreath.setBounds   (row.removeFromLeft (kw)); row.removeFromLeft (gap);
        knobSpark.setBounds    (row.removeFromLeft (kw));
    }

    // ── Loop/Actions card: LEADER header + ENGAGE, ACTIONS header + RESET ─
    {
        auto inner = loopCard.reduced (14, 12);

        leaderLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        // The ENGAGE toggle occupies the same vertical space as the loop row
        // on the melodic tabs (96 px) so all four tabs share the same card
        // geometry.
        const int engageH = 96;
        auto engageRow = inner.removeFromTop (engageH);
        engageButton.setBounds (engageRow.reduced (8, 20));

        inner.removeFromTop (10);

        actionsLabel.setBounds (inner.removeFromTop (kSectionHeaderH));
        inner.removeFromTop (4);

        auto actionsRow = inner.removeFromTop (kBigActionBtnH);
        resetButton.setBounds (actionsRow.reduced (8, 0));
    }

    // Mixer rows inside mixerArea.
    auto area = mixerArea.getLocalBounds().reduced (14, 12);
    const int nRows   = 4;
    const int rowGap  = 8;
    const int rowH    = juce::jmax (32, (area.getHeight() - rowGap * (nRows - 1)) / nRows);
    const int nameW   = 72;
    const int msW     = 28;
    const int msGap   = 6;
    const int msColW  = msW * 2 + msGap;
    const int prevGap = 10;

    auto place = [&] (Row& row)
    {
        auto rowB = area.removeFromTop (rowH);
        area.removeFromTop (rowGap);

        auto rb = rowB;
        row.name.setBounds (rb.removeFromLeft (nameW));
        rb.removeFromLeft (prevGap);

        auto msBlock = rb.removeFromRight (msColW);
        rb.removeFromRight (prevGap);
        row.preview->setBounds (rb.reduced (0, 4));

        const int btnH = juce::jmin (22, msBlock.getHeight() - 8);
        const int btnY = msBlock.getY() + (msBlock.getHeight() - btnH) / 2;
        row.mute.setBounds (msBlock.removeFromLeft (msW).withY (btnY).withHeight (btnH));
        msBlock.removeFromLeft (msGap);
        row.solo.setBounds (msBlock.removeFromLeft (msW).withY (btnY).withHeight (btnH));
    };

    place (animal);
    place (bootsy);
    place (stevie);
    place (paul);
}
