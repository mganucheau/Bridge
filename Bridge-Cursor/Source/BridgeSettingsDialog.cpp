#include "BridgeSettingsDialog.h"
#include "BridgeProcessor.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"
#include "PersonalityPresets.h"
#include "BridgeModelBundle.h"
#include "ml/BridgeMLManager.h"

namespace bridgeSettingsDetail
{

struct GeneralTab final : public juce::Component,
                          private juce::Timer
{
    BridgeProcessor& proc;

    juce::Label title { {}, "Bridge Settings" };
    juce::Label themeLabel { {}, "UI theme" };
    juce::ComboBox themeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> themeAttach;

    juce::Label midiRoutingLabel { {}, "MIDI outputs" };
    juce::TextEditor midiRoutingBody;

    juce::Label mlModelsSectionLabel;
    juce::Label mlDrumsName;
    juce::Label mlDrumsStatus;
    juce::Label mlBassName;
    juce::Label mlBassStatus;
    juce::Label mlChordsName;
    juce::Label mlChordsStatus;
    juce::Label mlMelodyName;
    juce::Label mlMelodyStatus;

    juce::Label mlBundleSectionLabel;
    juce::Label mlBundleVersionLabel;
    juce::TextButton mlInstallBundleButton { "Install from file…" };
    juce::TextButton mlCheckUpdatesButton { "Check for updates" };
    juce::Label mlUpdateBannerText;
    juce::TextButton mlUpdateDownloadButton { "Download" };
    std::unique_ptr<juce::FileChooser> mlBundleChooser;

    juce::Label aboutLabel;
    juce::TextEditor aboutBody;

    explicit GeneralTab (BridgeProcessor& p)
        : proc (p)
    {
        addAndMakeVisible (title);
        title.setFont (bridge::hig::uiFont (20.0f, "Semibold"));
        title.setColour (juce::Label::textColourId, bridge::colors::textPrimary());

        addAndMakeVisible (themeLabel);
        themeLabel.setFont (bridge::hig::uiFont (13.0f));
        themeLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());

        if (auto* par = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("uiTheme")))
        {
            const auto& names = par->choices;
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

        mlModelsSectionLabel.setText ("ML MODELS", juce::dontSendNotification);
        mlModelsSectionLabel.setFont (bridge::hig::uiFont (13.0f));
        mlModelsSectionLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
        addAndMakeVisible (mlModelsSectionLabel);

        auto setupMlName = [] (juce::Label& l, const juce::String& text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (bridge::hig::uiFont (12.0f));
            l.setColour (juce::Label::textColourId, bridge::colors::textPrimary());
        };
        setupMlName (mlDrumsName, "Drums");
        setupMlName (mlBassName, "Bass");
        setupMlName (mlChordsName, "Chords");
        setupMlName (mlMelodyName, "Melody");
        for (auto* l : { &mlDrumsName, &mlBassName, &mlChordsName, &mlMelodyName })
            addAndMakeVisible (*l);

        auto setupMlStatus = [] (juce::Label& l)
        {
            l.setFont (bridge::hig::uiFont (12.0f));
            l.setJustificationType (juce::Justification::centredRight);
        };
        setupMlStatus (mlDrumsStatus);
        setupMlStatus (mlBassStatus);
        setupMlStatus (mlChordsStatus);
        setupMlStatus (mlMelodyStatus);
        for (auto* l : { &mlDrumsStatus, &mlBassStatus, &mlChordsStatus, &mlMelodyStatus })
            addAndMakeVisible (*l);

        refreshMlModelStatusRows();

        mlBundleSectionLabel.setText ("Models", juce::dontSendNotification);
        mlBundleSectionLabel.setFont (bridge::hig::uiFont (13.0f));
        mlBundleSectionLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
        addAndMakeVisible (mlBundleSectionLabel);

        mlBundleVersionLabel.setFont (bridge::hig::uiFont (12.0f));
        mlBundleVersionLabel.setColour (juce::Label::textColourId, bridge::colors::textPrimary());
        addAndMakeVisible (mlBundleVersionLabel);

        addAndMakeVisible (mlInstallBundleButton);
        addAndMakeVisible (mlCheckUpdatesButton);

        mlUpdateBannerText.setFont (bridge::hig::uiFont (12.0f));
        mlUpdateBannerText.setColour (juce::Label::textColourId, bridge::colors::textPrimary());
        addChildComponent (mlUpdateBannerText);
        addChildComponent (mlUpdateDownloadButton);

        mlInstallBundleButton.onClick = [this]
        {
            mlBundleChooser = std::make_unique<juce::FileChooser> ("Install Bridge models",
                                                                   juce::File(),
                                                                   "*.bridgemodels");
            const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            mlBundleChooser->launchAsync (flags,
                                            [this] (const juce::FileChooser& fc)
                                            {
                                                const auto f = fc.getResult();
                                                if (f.existsAsFile())
                                                {
                                                    if (auto* ml = proc.getMLManager())
                                                    {
                                                        const auto r = BridgeModelBundle::installAndLoad (f, *ml);
                                                        if (! r.wasOk())
                                                            juce::AlertWindow::showMessageBoxAsync (
                                                                juce::AlertWindow::WarningIcon,
                                                                "Install failed",
                                                                r.getErrorMessage());
                                                    }
                                                }
                                                refreshMlModelStatusRows();
                                                refreshMlBundleRows();
                                                mlBundleChooser.reset();
                                            });
        };

        mlCheckUpdatesButton.onClick = [this] { proc.requestManualModelUpdateCheck(); };

        mlUpdateDownloadButton.onClick = [this]
        {
            const auto url = proc.getPendingModelUpdateUrl();
            if (url.isNotEmpty())
                juce::URL (url).launchInDefaultBrowser();
        };

        refreshMlBundleRows();

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

    void refreshMlModelStatusRows()
    {
        const juce::Colour loadedCol (0xFF6EE7A0);
        const juce::Colour missingCol (0xFF6A6578);

        auto apply = [&] (juce::Label& status, bool loaded)
        {
            status.setText (loaded ? "● Loaded" : "○ Not found", juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, loaded ? loadedCol : missingCol);
        };

        if (auto* ml = proc.getMLManager())
        {
            apply (mlDrumsStatus, ml->hasModel (BridgeMLManager::ModelType::DrumHumanizer));
            apply (mlBassStatus, ml->hasModel (BridgeMLManager::ModelType::BassModel));
            apply (mlChordsStatus, ml->hasModel (BridgeMLManager::ModelType::ChordsModel));
            apply (mlMelodyStatus, ml->hasModel (BridgeMLManager::ModelType::MelodyModel));
        }
        else
        {
            apply (mlDrumsStatus, false);
            apply (mlBassStatus, false);
            apply (mlChordsStatus, false);
            apply (mlMelodyStatus, false);
        }
    }

    void refreshMlBundleRows()
    {
        juce::String ver = BridgeModelBundle::getLoadedVersion();
        juce::String line;
        if (ver.isNotEmpty())
            line = "Bundle version: " + ver;
        else
        {
#if BRIDGE_HAS_MODEL_DATA
            line = "Bundle version: Built-in";
#else
            line = "Bundle version: —";
#endif
        }
        mlBundleVersionLabel.setText (line, juce::dontSendNotification);

        const bool showBanner = proc.getPendingModelUpdateUrl().isNotEmpty();
        mlUpdateBannerText.setVisible (showBanner);
        mlUpdateDownloadButton.setVisible (showBanner);
        if (showBanner)
        {
            mlUpdateBannerText.setText ("Model update " + proc.getPendingModelUpdateVersion()
                                            + " available —",
                                        juce::dontSendNotification);
        }
    }

    void visibilityChanged() override
    {
        if (isShowing())
            startTimerHz (1);
        else
            stopTimer();
    }

    void timerCallback() override
    {
        refreshMlBundleRows();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        title.setBounds (r.removeFromTop (28));
        r.removeFromTop (10);

        auto themeRow = r.removeFromTop (28);
        themeLabel.setBounds (themeRow.removeFromLeft (100));
        themeBox.setBounds (themeRow.removeFromLeft (320).reduced (0, 2));
        r.removeFromTop (12);

        midiRoutingLabel.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
        midiRoutingBody.setBounds (r.removeFromTop (72));
        r.removeFromTop (12);

        mlModelsSectionLabel.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        auto placeMlRow = [&] (juce::Label& name, juce::Label& status)
        {
            auto row = r.removeFromTop (22);
            name.setBounds (row.removeFromLeft (160));
            status.setBounds (row);
        };
        placeMlRow (mlDrumsName, mlDrumsStatus);
        placeMlRow (mlBassName, mlBassStatus);
        placeMlRow (mlChordsName, mlChordsStatus);
        placeMlRow (mlMelodyName, mlMelodyStatus);
        r.removeFromTop (10);

        mlBundleSectionLabel.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        mlBundleVersionLabel.setBounds (r.removeFromTop (22));
        auto btnRow = r.removeFromTop (30);
        mlInstallBundleButton.setBounds (btnRow.removeFromLeft (160).reduced (0, 2));
        btnRow.removeFromLeft (8);
        mlCheckUpdatesButton.setBounds (btnRow.removeFromLeft (160).reduced (0, 2));
        r.removeFromTop (6);

        if (mlUpdateBannerText.isVisible())
        {
            auto ban = r.removeFromTop (26);
            mlUpdateBannerText.setBounds (ban.removeFromLeft (juce::jmax (100, ban.getWidth() - 100)));
            mlUpdateDownloadButton.setBounds (ban.removeFromRight (88).reduced (0, 2));
            r.removeFromTop (8);
        }

        aboutLabel.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
        aboutBody.setBounds (r);
    }
};

struct PersonalityTab final : public juce::Component,
                              private juce::AudioProcessorValueTreeState::Listener
{
    BridgeProcessor& proc;

    juce::Label section { {}, "Personality" };
    juce::Label presetLabel { {}, "Load preset:" };
    juce::ComboBox presetCombo;
    juce::Label previewLabel { {}, "Preview instrument:" };
    juce::ComboBox previewInstrument;
    juce::Label hint;
    std::array<juce::Label, 10> names;
    std::array<juce::Slider, 10> sliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> attaches;

    bool applyingPreset = false;

    static constexpr const char* paramIds[10] = {
        "mlPersRhythmTight",
        "mlPersDynamicRange",
        "mlPersTimbreTexture",
        "mlPersTensionArc",
        "mlPersTempoVolatility",
        "mlPersEmotionalTemp",
        "mlPersHarmAdventure",
        "mlPersStructPredict",
        "mlPersShowmanship",
        "mlPersGenreLoyalty",
    };

    static constexpr const char* displayNames[10] = {
        "Rhythmic tightness",
        "Dynamic range",
        "Timbre texture",
        "Tension arc",
        "Tempo volatility",
        "Emotional temperature",
        "Harmonic adventurousness",
        "Structural predictability",
        "Showmanship",
        "Genre loyalty",
    };

    explicit PersonalityTab (BridgeProcessor& p)
        : proc (p)
    {
        addAndMakeVisible (section);
        section.setFont (bridge::hig::uiFont (15.0f, "Semibold"));
        section.setColour (juce::Label::textColourId, bridge::colors::textPrimary());

        addAndMakeVisible (presetLabel);
        presetLabel.setFont (bridge::hig::uiFont (12.0f));
        presetLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());

        presetCombo.addItem ("Custom", 1);
        for (size_t i = 0; i < bridge::personality::numNamedPresets; ++i)
            presetCombo.addItem (bridge::personality::kPresets[i].displayName, (int) i + 2);
        addAndMakeVisible (presetCombo);
        presetCombo.onChange = [this]
        {
            if (applyingPreset)
                return;
            const int idx = juce::jmax (0, presetCombo.getSelectedId() - 1);
            applyingPreset = true;
            if (auto* presetPar = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("mlPersonalityPresetName")))
                presetPar->setValueNotifyingHost (
                    presetPar->getNormalisableRange().convertTo0to1 ((float) idx));

            if (idx > 0 && (size_t) idx <= bridge::personality::numNamedPresets)
            {
                const auto& e = bridge::personality::kPresets[(size_t) idx - 1];
                for (int i = 0; i < 10; ++i)
                    if (auto* par = proc.apvtsMain.getParameter (paramIds[(size_t) i]))
                        par->setValueNotifyingHost (e.knobs[(size_t) i]);
            }
            applyingPreset = false;
        };

        addAndMakeVisible (previewLabel);
        previewLabel.setFont (bridge::hig::uiFont (12.0f));
        previewLabel.setColour (juce::Label::textColourId, bridge::colors::textSecondary());

        previewInstrument.addItem ("Drums", 1);
        previewInstrument.addItem ("Bass", 2);
        previewInstrument.addItem ("Chords", 3);
        previewInstrument.addItem ("Melody", 4);
        previewInstrument.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (previewInstrument);

        hint.setText ("These 10 values are shared across all ONNX personality models (0–1). "
                      "Preview instrument is display-only — sliders always update the same parameters.",
                      juce::dontSendNotification);
        hint.setFont (bridge::hig::uiFont (11.0f));
        hint.setColour (juce::Label::textColourId, bridge::colors::textSecondary());
        hint.setMinimumHorizontalScale (0.85f);
        addAndMakeVisible (hint);

        for (int i = 0; i < 10; ++i)
        {
            names[(size_t) i].setText (displayNames[(size_t) i], juce::dontSendNotification);
            names[(size_t) i].setFont (bridge::hig::uiFont (12.0f));
            names[(size_t) i].setColour (juce::Label::textColourId, bridge::colors::textPrimary());
            addAndMakeVisible (names[(size_t) i]);

            auto& sl = sliders[(size_t) i];
            sl.setSliderStyle (juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
            sl.setRange (0.0, 1.0, 0.01);
            addAndMakeVisible (sl);

            attaches[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvtsMain, paramIds[(size_t) i], sl);
        }

        for (int i = 0; i < 10; ++i)
            proc.apvtsMain.addParameterListener (paramIds[(size_t) i], this);
        proc.apvtsMain.addParameterListener ("mlPersonalityPresetName", this);

        if (auto* presetPar = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("mlPersonalityPresetName")))
            presetCombo.setSelectedId (presetPar->getIndex() + 1, juce::dontSendNotification);
    }

    ~PersonalityTab() override
    {
        for (int i = 0; i < 10; ++i)
            proc.apvtsMain.removeParameterListener (paramIds[(size_t) i], this);
        proc.apvtsMain.removeParameterListener ("mlPersonalityPresetName", this);
    }

    void parameterChanged (const juce::String& parameterID, float newValue) override
    {
        juce::ignoreUnused (newValue);
        if (applyingPreset)
            return;

        if (parameterID == "mlPersonalityPresetName")
        {
            applyingPreset = true;
            if (auto* presetPar = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvtsMain.getParameter ("mlPersonalityPresetName")))
            {
                const int idx = presetPar->getIndex();
                presetCombo.setSelectedId (idx + 1, juce::dontSendNotification);
                if (idx > 0 && (size_t) idx <= bridge::personality::numNamedPresets)
                {
                    const auto& e = bridge::personality::kPresets[(size_t) idx - 1];
                    for (int i = 0; i < 10; ++i)
                        if (auto* par = proc.apvtsMain.getParameter (paramIds[(size_t) i]))
                            par->setValueNotifyingHost (e.knobs[(size_t) i]);
                }
            }
            applyingPreset = false;
            return;
        }

        for (int i = 0; i < 10; ++i)
        {
            if (parameterID == paramIds[(size_t) i])
            {
                std::array<float, 10> knobs{};
                for (int k = 0; k < 10; ++k)
                    if (auto* rv = proc.apvtsMain.getRawParameterValue (paramIds[(size_t) k]))
                        knobs[(size_t) k] = rv->load();
                const int idx = bridge::personality::indexMatchingKnobs (knobs);
                applyingPreset = true;
                if (auto* presetPar = dynamic_cast<juce::AudioParameterChoice*> (
                        proc.apvtsMain.getParameter ("mlPersonalityPresetName")))
                    presetPar->setValueNotifyingHost (
                    presetPar->getNormalisableRange().convertTo0to1 ((float) idx));
                presetCombo.setSelectedId (idx + 1, juce::dontSendNotification);
                applyingPreset = false;
                return;
            }
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        section.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);
        auto presetRow = r.removeFromTop (28);
        presetLabel.setBounds (presetRow.removeFromLeft (180));
        presetCombo.setBounds (presetRow.removeFromLeft (280).reduced (0, 2));
        r.removeFromTop (6);
        auto previewRow = r.removeFromTop (28);
        previewLabel.setBounds (previewRow.removeFromLeft (180));
        previewInstrument.setBounds (previewRow.removeFromLeft (240).reduced (0, 2));
        r.removeFromTop (6);
        hint.setBounds (r.removeFromTop (40));
        r.removeFromTop (8);

        for (int i = 0; i < 10; ++i)
        {
            auto row = r.removeFromTop (30);
            names[(size_t) i].setBounds (row.removeFromLeft (200));
            sliders[(size_t) i].setBounds (row.reduced (4, 2));
        }
    }
};

} // namespace bridgeSettingsDetail

BridgeSettingsDialog::BridgeSettingsDialog (BridgeProcessor& processor)
    : proc (processor)
{
    auto* general = new bridgeSettingsDetail::GeneralTab (proc);
    auto* personality = new bridgeSettingsDetail::PersonalityTab (proc);
    tabs.addTab ("General", juce::Colours::transparentBlack, general, true);
    tabs.addTab ("Personality", juce::Colours::transparentBlack, personality, true);
    tabs.setCurrentTabIndex (0);
    addAndMakeVisible (tabs);
}

void BridgeSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (bridge::colors::background());
}

void BridgeSettingsDialog::resized()
{
    tabs.setBounds (getLocalBounds());
}
