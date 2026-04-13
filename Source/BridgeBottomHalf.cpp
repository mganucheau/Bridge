#include "BridgeBottomHalf.h"
#include "BridgeLoopRangeStrip.h"
#include "BridgePanelLayout.h"

LabelledKnob::LabelledKnob (const juce::String& paramId, const juce::String& name,
                            juce::AudioProcessorValueTreeState& apvts, BridgeLookAndFeel::KnobStyle style, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    BridgeLookAndFeel::setKnobStyle (slider, style, accent);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredTop);
    label.setColour (juce::Label::textColourId, bridge::colors::textDim);
    label.setFont (juce::Font (juce::FontOptions().withHeight (10.0f).withStyle ("SemiBold")));
    addAndMakeVisible (label);

    auto* param = apvts.getParameter (paramId);
    if (param != nullptr)
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (16));
    slider.setBounds (b.withSizeKeepingCentre (juce::jmin (b.getWidth(), b.getHeight()), juce::jmin (b.getWidth(), b.getHeight())));
}

// ----------------------------------------------------

BridgeBottomHalf::BridgeBottomHalf (juce::AudioProcessorValueTreeState& apvtsToUse,
                                    BridgeLookAndFeel& laf,
                                    juce::Colour groupAccent,
                                    std::function<void()> onGenerate,
                                    std::function<void(bool)> onFillHold)
    : apvts (apvtsToUse),
      knobDensity ("density", "DENSITY", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobSwing ("swing", "SWING", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobHumanize ("humanize", "HUMANIZE", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobPocket ("pocket", "POCKET", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobVelocity ("velocity", "VELOCITY", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobFillRate ("fillRate", "FILL RATE", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobComplexity ("complexity", "COMPLEXITY", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobGhost ("ghostAmount", "GHOST", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobPresence ("presence", "PRESENCE", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobLoopStart ("loopStart", "START", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      knobLoopEnd ("loopEnd", "END", apvts, BridgeLookAndFeel::KnobStyle::BigRing, groupAccent),
      fillHoldCallback (onFillHold)
{
    setLookAndFeel (&laf);

    auto setupHeader = [&] (juce::Label& l, const juce::String& text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Semibold")));
        l.setColour (juce::Label::textColourId, bridge::colors::textDim);
        addAndMakeVisible (l);
    };

    setupHeader (grooveLabel, "I  GROOVE");
    setupHeader (expressionLabel, "I  EXPRESSION");
    setupHeader (loopingLabel, "I  LOOPING");
    setupHeader (actionsLabel, "I  ACTIONS");

    // Add knobs to view
    addAndMakeVisible (knobDensity);
    addAndMakeVisible (knobSwing);
    addAndMakeVisible (knobHumanize);
    addAndMakeVisible (knobPocket);
    addAndMakeVisible (knobVelocity);
    
    addAndMakeVisible (knobFillRate);
    addAndMakeVisible (knobComplexity);
    addAndMakeVisible (knobGhost);
    addAndMakeVisible (knobPresence);

    addAndMakeVisible (knobLoopStart);
    addAndMakeVisible (knobLoopEnd);

    loopRangeStrip = std::make_unique<BridgeLoopRangeStrip> (apvts, groupAccent, 16);
    addAndMakeVisible (*loopRangeStrip);

    juce::Path syncGlyph;
    syncGlyph.addEllipse (3.0f, 3.0f, 18.0f, 18.0f);
    syncGlyph.addEllipse (8.0f, 8.0f, 8.0f, 8.0f);
    syncGlyph.setUsingNonZeroWinding (false);
    syncIconButton.setShape (syncGlyph, true, true, false);
    syncIconButton.setClickingTogglesState (true);
    syncIconButton.shouldUseOnColours (true);
    syncIconButton.setColours (bridge::colors::knobTrack, bridge::colors::knobTrack, bridge::colors::knobTrack);
    syncIconButton.setOnColours (groupAccent.withAlpha (0.75f), groupAccent.brighter (0.1f), groupAccent);
    syncIconButton.setOutline (juce::Colour (0xff9e99a8), 1.0f);
    syncIconButton.setTooltip ("Loop sync (loop width lock when on)");
    if (apvts.getParameter ("loopOn") != nullptr)
        syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "loopOn", syncIconButton);
    addAndMakeVisible (syncIconButton);

    apvts.addParameterListener ("loopStart", this);
    apvts.addParameterListener ("loopEnd", this);

    // Action buttons
    generateButton.onClick = onGenerate;
    fillButton.addMouseListener (&fillListener, false);
    performButton.setClickingTogglesState (true);
    
    // Style actions like pads
    auto setupAction = [&] (juce::TextButton& btn) {
        btn.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack);
        btn.setColour (juce::TextButton::textColourOffId, bridge::colors::textDim);
        addAndMakeVisible(btn);
    };
    setupAction (generateButton);
    setupAction (fillButton);
    setupAction (performButton);
    
    performButton.setColour (juce::TextButton::buttonColourId, bridge::colors::knobTrack);
    performButton.setColour (juce::TextButton::buttonOnColourId, groupAccent.withAlpha (0.4f));
    performButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    if (apvts.getParameter ("perform") != nullptr)
        performAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "perform", performButton);
}

BridgeBottomHalf::~BridgeBottomHalf()
{
    apvts.removeParameterListener ("loopStart", this);
    apvts.removeParameterListener ("loopEnd", this);
    setLookAndFeel (nullptr);
}

void BridgeBottomHalf::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);
    if (parameterID == "loopStart" || parameterID == "loopEnd")
        if (loopRangeStrip != nullptr)
            loopRangeStrip->repaint();
}

void BridgeBottomHalf::paint (juce::Graphics& g)
{
    using namespace bridge::colors;
    
    auto bounds = getLocalBounds();
    auto shell = bridge::panelLayout::splitInstrumentContent (bounds, 0); // No top trim inside the component itself

    // Draw generic cards
    auto drawCard = [&] (juce::Rectangle<int> r) {
        g.setColour (cardSurface);
        g.fillRoundedRectangle (r.toFloat(), bridge::instrumentLayout::kCardRadius);
        g.setColour (cardOutline.withAlpha (0.35f));
        g.drawRoundedRectangle (r.toFloat().reduced(0.5f), bridge::instrumentLayout::kCardRadius, 1.0f);
    };

    drawCard (shell.knobsCard);
    drawCard (shell.loopActionsCard);
    
    // Subtle separator line in Knobs Card
    g.setColour (cardOutline.withAlpha(0.6f));
    auto sep = shell.knobsCard.withTrimmedLeft(16).withTrimmedRight(16);
    g.fillRect (sep.getX(), sep.getCentreY(), sep.getWidth(), 1);
}

void BridgeBottomHalf::resized()
{
    auto bounds = getLocalBounds();
    auto shell = bridge::panelLayout::splitInstrumentContent (bounds, 0);

    // ── Knobs Card (GROOVE / EXPRESSION) ──
    auto knobsCard = shell.knobsCard.reduced (16, 16);
    
    // Groove Row (5 Knobs)
    auto topHalf = knobsCard.removeFromTop (knobsCard.getHeight() / 2 - 4);
    knobsCard.removeFromTop (8); // separator gap
    
    grooveLabel.setBounds (topHalf.removeFromTop (16));
    topHalf.removeFromTop (4);
    
    int w = topHalf.getWidth() / 5;
    knobDensity.setBounds (topHalf.removeFromLeft (w));
    knobSwing.setBounds (topHalf.removeFromLeft (w));
    knobHumanize.setBounds (topHalf.removeFromLeft (w));
    knobPocket.setBounds (topHalf.removeFromLeft (w));
    knobVelocity.setBounds (topHalf.removeFromLeft (w));

    // Expression Row (4 Knobs)
    auto botHalf = knobsCard;
    expressionLabel.setBounds (botHalf.removeFromTop (16));
    botHalf.removeFromTop (4);
    
    w = botHalf.getWidth() / 5; // Use 5 for scaling consistency so they physically match Groove
    knobFillRate.setBounds (botHalf.removeFromLeft (w));
    knobComplexity.setBounds (botHalf.removeFromLeft (w));
    knobGhost.setBounds (botHalf.removeFromLeft (w));
    knobPresence.setBounds (botHalf.removeFromLeft (w));

    // ── Looping / Actions Card ──
    auto loopCard = shell.loopActionsCard.reduced (16, 16);
    
    // Looping Row
    auto loopHalf = loopCard.removeFromTop (loopCard.getHeight() / 2 - 4);
    loopCard.removeFromTop (8);
    
    loopingLabel.setBounds (loopHalf.removeFromTop (16));
    loopHalf.removeFromTop (4);
    if (loopRangeStrip != nullptr)
        loopRangeStrip->setBounds (loopHalf.removeFromTop (34));
    loopHalf.removeFromTop (6);

    const int syncW = 34;
    const int knobW = (loopHalf.getWidth() - syncW - 16) / 2;
    knobLoopStart.setBounds (loopHalf.removeFromLeft (knobW));
    loopHalf.removeFromLeft (8);
    syncIconButton.setBounds (loopHalf.removeFromLeft (syncW).withSizeKeepingCentre (syncW, syncW));
    loopHalf.removeFromLeft (8);
    knobLoopEnd.setBounds (loopHalf.removeFromLeft (knobW));
    
    // Actions Row
    auto actionsHalf = loopCard;
    actionsLabel.setBounds (actionsHalf.removeFromTop (16));
    actionsHalf.removeFromTop (4);
    
    auto btnGap = 10;
    auto btnW = (actionsHalf.getWidth() - btnGap * 2) / 3;
    generateButton.setBounds (actionsHalf.removeFromLeft (btnW));
    actionsHalf.removeFromLeft (btnGap);
    fillButton.setBounds (actionsHalf.removeFromLeft (btnW));
    actionsHalf.removeFromLeft (btnGap);
    performButton.setBounds (actionsHalf.removeFromLeft (btnW));
}
