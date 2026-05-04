#include "BridgeXYPad.h"
#include "BridgeAppleHIG.h"
#include "BridgeLookAndFeel.h"

BridgeXYPad::BridgeXYPad (juce::AudioProcessorValueTreeState& apvtsIn,
                          const juce::String& paramIdX,
                          const juce::String& paramIdY,
                          juce::Colour accentColour)
    : apvts (apvtsIn), pidX (paramIdX), pidY (paramIdY), accent (accentColour)
{
    apvts.addParameterListener (pidX, this);
    apvts.addParameterListener (pidY, this);
    setOpaque (false);
}

BridgeXYPad::~BridgeXYPad()
{
    apvts.removeParameterListener (pidX, this);
    apvts.removeParameterListener (pidY, this);
}

juce::Rectangle<float> BridgeXYPad::padBounds() const
{
    return getLocalBounds().toFloat().reduced (4.0f);
}

void BridgeXYPad::syncFromParameters()
{
    repaint();
}

void BridgeXYPad::parameterChanged (const juce::String& id, float)
{
    if (id == pidX || id == pidY)
        repaint();
}

void BridgeXYPad::setFromPosition (juce::Point<float> posInLocal)
{
    auto b = padBounds();
    if (b.getWidth() <= 1.0f || b.getHeight() <= 1.0f) return;

    float nx = (posInLocal.x - b.getX()) / b.getWidth();
    float ny = 1.0f - (posInLocal.y - b.getY()) / b.getHeight();
    nx = juce::jlimit (0.0f, 1.0f, nx);
    ny = juce::jlimit (0.0f, 1.0f, ny);

    if (auto* px = apvts.getParameter (pidX))
        px->setValueNotifyingHost (nx);
    if (auto* py = apvts.getParameter (pidY))
        py->setValueNotifyingHost (ny);
}

void BridgeXYPad::mouseDown (const juce::MouseEvent& e) { dragging = true; setFromPosition (e.position); }
void BridgeXYPad::mouseDrag (const juce::MouseEvent& e) { if (dragging) setFromPosition (e.position); }

void BridgeXYPad::paint (juce::Graphics& g)
{
    auto b = padBounds();
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (b);
    g.setColour (bridge::colors::cardOutline().withAlpha (0.5f));
    g.drawRect (b.reduced (0.5f), 1.0f);

    float x = 0.5f, y = 0.5f;
    if (apvts.getRawParameterValue (pidX) != nullptr)
        x = apvts.getRawParameterValue (pidX)->load();
    if (apvts.getRawParameterValue (pidY) != nullptr)
        y = apvts.getRawParameterValue (pidY)->load();

    float px = b.getX() + x * b.getWidth();
    float py = b.getY() + (1.0f - y) * b.getHeight();

    g.setColour (bridge::colors::cardOutline().withAlpha (0.4f));
    g.drawLine (b.getX(), py, b.getRight(), py, 1.0f);
    g.drawLine (px, b.getY(), px, b.getBottom(), 1.0f);

    const float dotR = 6.0f;
    g.setColour (accent.withAlpha (0.95f));
    g.fillEllipse (px - dotR, py - dotR, dotR * 2.0f, dotR * 2.0f);
    g.setColour (bridge::hig::label.withAlpha (0.35f));
    g.drawEllipse (px - dotR, py - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
}

void BridgeXYPad::resized() {}
