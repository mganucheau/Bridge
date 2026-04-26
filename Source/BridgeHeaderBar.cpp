#include "BridgeHeaderBar.h"
#include "BridgeAppleHIG.h"
#include "BridgeInstrumentStyles.h"
#include "BridgeLookAndFeel.h"
#include "BridgeProcessor.h"
#include "BridgeSettingsDialog.h"

namespace
{
/** Minimum width for combo + longest item text (arrow/padding). */
static int measuredComboWidth (juce::ComboBox& box, int minW, int extraPad = 28)
{
    float w = 0.0f;
    auto f = box.getLookAndFeel().getComboBoxFont (box);
    for (int i = 0; i < box.getNumItems(); ++i)
        w = juce::jmax (w, (float) f.getStringWidth (box.getItemText (i)));
    return juce::jmax (minW, (int) std::ceil (w) + extraPad);
}

static bool paramOn (juce::AudioProcessorValueTreeState& ap, const char* id)
{
    if (auto* v = ap.getRawParameterValue (id))
        return v->load() > 0.5f;
    return true;
}

static void setupTransportBtn (juce::ShapeButton& b)
{
    b.setColours (juce::Colours::white, juce::Colours::white, juce::Colours::white);
    b.setOutline (juce::Colours::white.withAlpha (0.2f), 1.0f);
}

/** Gear icon path for 18×18 button (half of former 36px control). */
static juce::Path makeGearPathForSmallButton()
{
    juce::Path p;
    p.addStar (juce::Point<float> (16.0f, 16.0f), 8, 5.5f, 8.5f, 0.0f);
    p.addEllipse (10.0f, 10.0f, 12.0f, 12.0f);
    p.setUsingNonZeroWinding (false);
    const float s = 9.0f / 32.0f;
    p.applyTransform (juce::AffineTransform::scale (s).translated (4.5f, 4.5f));
    return p;
}
} // namespace

BridgeHeaderBar::BridgeHeaderBar (BridgeProcessor& processor,
                                  std::function<void (int tabIndex)> onSelectInstrumentTab)
    : proc (processor)
{
    setOpaque (true);

    addAndMakeVisible (goldBar);

    logoLabel.setText ("BRIDGE", juce::dontSendNotification);
    {
        auto f = bridge::hig::uiFont (17.0f, "Semibold");
        f = f.withExtraKerningFactor (0.08f);
        logoLabel.setFont (f);
    }
    logoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    bpmValueLabel.setText ("120", juce::dontSendNotification);
    bpmValueLabel.setFont (bridge::hig::uiFont (14.0f));
    bpmValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    bpmValueLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff2a2a2a));
    bpmValueLabel.setColour (juce::Label::outlineColourId, juce::Colours::white.withAlpha (0.2f));
    bpmValueLabel.setJustificationType (juce::Justification::centred);
    bpmValueLabel.setEditable (true, false, false);
    bpmValueLabel.setOpaque (true);
    bpmValueLabel.setBorderSize (juce::BorderSize<int> (1, 6, 1, 6));
    bpmValueLabel.onTextChange = [this]
    {
        if (proc.apvtsMain.getRawParameterValue ("internalBpm") != nullptr)
        {
            auto newVal = bpmValueLabel.getText().getFloatValue();
            newVal = juce::jlimit (20.0f, 300.0f, newVal);
            proc.apvtsMain.getParameter ("internalBpm")->setValueNotifyingHost (
                proc.apvtsMain.getParameter ("internalBpm")->getNormalisableRange().convertTo0to1 (newVal));
        }
    };
    addAndMakeVisible (bpmValueLabel);

    bpmUnitLabel.setText ("BPM", juce::dontSendNotification);
    bpmUnitLabel.setFont (bridge::hig::uiFont (12.0f));
    bpmUnitLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    bpmUnitLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmUnitLabel);

    juce::Path playPath;
    playPath.addTriangle (6.0f, 4.5f, 6.0f, 11.5f, 12.0f, 8.0f);
    playButton.setShape (playPath, true, true, false);
    playButton.setClickingTogglesState (true);
    playButton.shouldUseOnColours (true);
    playButton.setOnColours (juce::Colours::white, juce::Colours::white, juce::Colours::white);
    playButton.setName ("bridgePlayBtn");
    setupTransportBtn (playButton);
    transportAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvtsMain, "transportPlaying", playButton);
    addAndMakeVisible (playButton);
    playButton.setLookAndFeel (&chromeLaf);

    juce::Path stopPath;
    stopPath.addRectangle (6.0f, 6.0f, 5.0f, 5.0f);
    stopButton.setShape (stopPath, true, true, false);
    stopButton.setClickingTogglesState (false);
    stopButton.setName ("bridgeStopBtn");
    setupTransportBtn (stopButton);
    stopButton.onClick = [this]
    {
        if (auto* pr = proc.apvtsMain.getParameter ("transportPlaying"))
            pr->setValueNotifyingHost (0.0f);
    };
    addAndMakeVisible (stopButton);
    stopButton.setLookAndFeel (&chromeLaf);

    hostSyncButton.setName ("bridgeHostSyncBtn");
    hostSyncButton.setClickingTogglesState (true);
    hostSyncButton.setConnectedEdges (0);
    hostSyncButton.setMouseClickGrabsKeyboardFocus (false);
    hostSyncButton.setWantsKeyboardFocus (false);
    hostSyncButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    hostSyncButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    hostSyncButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    hostSyncButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    hostSyncButton.setTooltip ("Sync tempo to host");
    hostSyncButton.setLookAndFeel (&chromeLaf);
    hostSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvtsMain, "hostSync", hostSyncButton);
    addAndMakeVisible (hostSyncButton);

    timeDivisionBox.getProperties().set ("bridgeHeaderStrip", true);
    timeDivisionBox.setLookAndFeel (&chromeLaf);
    timeDivisionBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2a));
    timeDivisionBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    timeDivisionBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.2f));
    timeDivisionBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.55f));
    timeDivisionBox.addItem ("1/64", 1);
    timeDivisionBox.addItem ("1/32", 2);
    timeDivisionBox.addItem ("1/24 T", 3);
    timeDivisionBox.addItem ("1/16", 4);
    timeDivisionBox.addItem ("1/12 T", 5);
    timeDivisionBox.addItem ("1/8", 6);
    timeDivisionBox.addItem ("1/6 T", 7);
    timeDivisionBox.addItem ("1/4", 8);
    timeDivisionBox.addItem ("1/2", 9);
    timeDivisionBox.addItem ("1 bar", 10);
    timeDivisionBox.setTooltip ("Grid / step length");
    addAndMakeVisible (timeDivisionBox);
    if (proc.apvtsMain.getParameter ("timeDivision") != nullptr)
        timeDivisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvtsMain, "timeDivision", timeDivisionBox);

    phraseBarsBox.getProperties().set ("bridgeHeaderStrip", true);
    phraseBarsBox.setLookAndFeel (&chromeLaf);
    phraseBarsBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2a));
    phraseBarsBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    phraseBarsBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.2f));
    phraseBarsBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.55f));
    phraseBarsBox.addItem ("2 bars", 1);
    phraseBarsBox.addItem ("4 bars", 2);
    phraseBarsBox.addItem ("8 bars", 3);
    phraseBarsBox.addItem ("16 bars", 4);
    phraseBarsBox.setTooltip ("Phrase length: how many bars the grid shows and loops");
    addAndMakeVisible (phraseBarsBox);
    if (proc.apvtsMain.getParameter ("phraseBars") != nullptr)
        phraseBarsAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvtsMain, "phraseBars", phraseBarsBox);

    tabRail = std::make_unique<BridgeInstrumentTabRail> (proc, std::move (onSelectInstrumentTab));
    addAndMakeVisible (*tabRail);

    {
        juce::Path gearPath = makeGearPathForSmallButton();
        gearButton.setShape (gearPath, true, true, false);
    }
    gearButton.setColours (juce::Colours::white, juce::Colours::white, juce::Colours::white);
    gearButton.setOnColours (juce::Colours::white, juce::Colours::white, juce::Colours::white);
    gearButton.setOutline (juce::Colours::transparentBlack, 0.0f);
    gearButton.setName ("bridgeGearBtn");
    gearButton.onClick = [this]
    {
        auto* content = new BridgeSettingsDialog (proc);
        content->setSize (540, 780);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (content);
        o.dialogTitle                 = "Settings";
        o.dialogBackgroundColour      = bridge::colors::background();
        o.useNativeTitleBar           = true;
        o.resizable                   = false;
        o.useBottomRightCornerResizer = false;
        o.launchAsync();
    };
    addAndMakeVisible (gearButton);
    gearButton.setLookAndFeel (&chromeLaf);
}

BridgeHeaderBar::~BridgeHeaderBar()
{
    gearButton.setLookAndFeel (nullptr);
    playButton.setLookAndFeel (nullptr);
    stopButton.setLookAndFeel (nullptr);
    hostSyncButton.setLookAndFeel (nullptr);
    timeDivisionBox.setLookAndFeel (nullptr);
    phraseBarsBox.setLookAndFeel (nullptr);
}

void BridgeHeaderBar::paint (juce::Graphics& g)
{
    auto rf = getLocalBounds().toFloat();
    g.setColour (juce::Colour (bridge::headerSpec::kBarBg));
    g.fillRect (rf);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawLine (rf.getX(), rf.getBottom() - 0.5f, rf.getRight(), rf.getBottom() - 0.5f, 1.0f);
}

void BridgeHeaderBar::resized()
{
    using namespace bridge::headerSpec;

    auto area = getLocalBounds();
    const int headerCy = area.getCentreY();
    const int cy0 = headerCy - kCtrlRowH / 2;

    const int leftX = kPadX;
    goldBar.setBounds (leftX, headerCy - kGoldBarH / 2, kGoldBarW, kGoldBarH);

    const int logoX = goldBar.getRight() + kGapAfterGold;
    logoLabel.setBounds (logoX, cy0, 120, kCtrlRowH);

    const int wordmarkW = 100;
    const int leftBlockEnd = logoX + wordmarkW;

    juce::GlyphArrangement ga;
    ga.addLineOfText (bridge::hig::uiFont (12.0f), "Host sync", 0.0f, 0.0f);
    const int hostW = juce::jmax (88, (int) std::ceil (ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth()) + 16);

    constexpr int transportGap = kTransportGap;
    const int trBtn = kTransportBtnSide;
    const int divW    = measuredComboWidth (timeDivisionBox, kDivComboW);
    const int phraseW = measuredComboWidth (phraseBarsBox, kPhraseComboW);
    const int centerW = kBpmFieldW + transportGap + kBpmLabelW + transportGap + trBtn + transportGap + trBtn
                        + transportGap + divW + transportGap + phraseW + transportGap + hostW;

    const int tabsW = BridgeInstrumentTabRail::contentWidth();
    const int rightClusterW = kTabToSettingsGap + tabsW + kGearSide;
    const int rightStart = area.getRight() - kPadX - rightClusterW;

    int cx = area.getCentreX() - centerW / 2;
    const int minCx = leftBlockEnd + transportGap;
    const int maxCx = rightStart - transportGap - centerW;
    cx = juce::jlimit (minCx, maxCx, cx);

    {
        auto row = juce::Rectangle<int> (cx, cy0, centerW, kCtrlRowH);
        bpmValueLabel.setBounds (row.removeFromLeft (kBpmFieldW));
        row.removeFromLeft (transportGap);
        bpmUnitLabel.setBounds (row.removeFromLeft (kBpmLabelW).withHeight (kCtrlRowH));
        row.removeFromLeft (transportGap);
        playButton.setBounds (row.removeFromLeft (trBtn));
        stopButton.setBounds (row.removeFromLeft (trBtn));
        row.removeFromLeft (transportGap);
        timeDivisionBox.setBounds (row.removeFromLeft (divW));
        row.removeFromLeft (transportGap);
        phraseBarsBox.setBounds (row.removeFromLeft (phraseW));
        row.removeFromLeft (transportGap);
        hostSyncButton.setBounds (row.removeFromLeft (hostW));
    }

    const int xTabs = area.getRight() - kPadX - kGearSide - kTabToSettingsGap - tabsW;
    if (tabRail != nullptr)
        tabRail->setBounds (xTabs, (getHeight() - 38) / 2, tabsW, 38);

    gearButton.setBounds (area.getRight () - kPadX - kGearSide, headerCy - kGearSide / 2,
                          kGearSide, kGearSide);
}

void BridgeHeaderBar::syncInstrumentTabSelection (int activeTabIndex)
{
    if (tabRail != nullptr)
        tabRail->setSelectedTab (activeTabIndex);
}

void BridgeHeaderBar::syncBpmDisplay()
{
    if (bpmValueLabel.isBeingEdited())
        return;

    const bool hostOn = paramOn (proc.apvtsMain, "hostSync");
    float bpm = 120.0f;
    if (hostOn)
        bpm = (float) proc.currentHostBpm.load();
    else if (auto* v = proc.apvtsMain.getRawParameterValue ("internalBpm"))
        bpm = v->load();

    bpmValueLabel.setText (juce::String ((int) std::round (bpm)), juce::dontSendNotification);
}

bool BridgeHeaderBar::bpmValueEditorHasKeyboardFocus() const
{
    return bpmValueLabel.hasKeyboardFocus (true);
}
