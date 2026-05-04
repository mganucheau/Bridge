#include "BridgeEditorChromeLookAndFeel.h"

juce::Colour BridgeEditorChromeLookAndFeel::chromeFieldBg()    { return juce::Colour (0xff2a2a2a); }
juce::Colour BridgeEditorChromeLookAndFeel::chromeFieldHover() { return juce::Colour (0xff3a3a3a); }
juce::Colour BridgeEditorChromeLookAndFeel::chromeBorder()    { return juce::Colours::white.withAlpha (0.2f); }

void BridgeEditorChromeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                         const juce::Colour& backgroundColour,
                                                         bool shouldDrawButtonAsHighlighted,
                                                         bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);
    auto r = button.getLocalBounds().toFloat();
    const auto& nm = button.getName();

    if (dynamic_cast<juce::ShapeButton*> (&button) != nullptr)
    {
        if (nm == "bridgeGearBtn")
        {
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect (r);
            }
            return;
        }

        if (nm == "bridgePlayBtn")
        {
            juce::Colour fill = chromeFieldBg();
            if (button.getToggleState())
                fill = juce::Colour (0xff22c55e);
            else if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
                fill = chromeFieldHover();

            g.setColour (fill);
            g.fillRect (r);
            g.setColour (chromeBorder());
            g.drawRect (r.reduced (0.5f), 1.0f);
            return;
        }

        if (nm == "bridgeStopBtn")
        {
            juce::Colour fill = chromeFieldBg();
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
                fill = chromeFieldHover();
            g.setColour (fill);
            g.fillRect (r);
            g.setColour (chromeBorder());
            g.drawRect (r.reduced (0.5f), 1.0f);
            return;
        }

        return;
    }

    if (nm == "bridgeHostSyncBtn")
    {
        juce::Colour fill = button.getToggleState() ? chromeFieldHover() : chromeFieldBg();
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            fill = chromeFieldHover();
        g.setColour (fill);
        g.fillRect (r);
        g.setColour (chromeBorder());
        g.drawRect (r.reduced (0.5f), 1.0f);
        return;
    }

    LookAndFeel_V4::drawButtonBackground (g, button, backgroundColour,
                                          shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void BridgeEditorChromeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    if (button.getName() == "bridgeHostSyncBtn")
    {
        g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
        g.setColour (juce::Colours::white);
        g.drawText (button.getButtonText(), button.getLocalBounds().reduced (8, 4),
                    juce::Justification::centred, false);
        return;
    }

    LookAndFeel_V4::drawButtonText (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void BridgeEditorChromeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                                  int buttonX, int buttonY, int buttonW, int buttonH,
                                                  juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);
    if (! box.getProperties()["bridgeHeaderStrip"])
    {
        LookAndFeel_V4::drawComboBox (g, width, height, isButtonDown, buttonX, buttonY, buttonW, buttonH, box);
        return;
    }

    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    juce::Colour fill = box.findColour (juce::ComboBox::backgroundColourId);
    if (box.isMouseOver (true))
        fill = chromeFieldHover();

    g.setColour (fill);
    g.fillRect (bounds);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRect (bounds.reduced (0.5f), 1.0f);

    juce::Path arrow;
    arrow.addTriangle (0.0f, 0.0f, 8.0f, 0.0f, 4.0f, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow, juce::AffineTransform::translation ((float) width - 14.0f, (float) height * 0.5f - 2.5f));
}

void BridgeEditorChromeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    if (box.getProperties()["bridgeHeaderStrip"])
    {
        label.setBounds (6, 0, box.getWidth() - 28, box.getHeight());
        label.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        return;
    }

    LookAndFeel_V4::positionComboBoxText (box, label);
}
