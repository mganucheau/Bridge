#include "BridgeEditor.h"
#include "BridgeHeaderBar.h"
#include "BridgeArrangementStrip.h"
#include "BridgeInstrumentStyles.h"
#include "BridgeLookAndFeel.h"
#include "drums/DrumsLookAndFeel.h"
#include "bass/BassLookAndFeel.h"
#include "piano/PianoLookAndFeel.h"
#include "guitar/GuitarLookAndFeel.h"

BridgeEditor::BridgeEditor (BridgeProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      mainPanel (p),
      drumsPanel (p),
      bassPanel (p),
      pianoPanel (p),
      guitarPanel (p)
{
    using namespace bridge::instrumentLayout;

    addChildComponent (mainPanel);
    addChildComponent (drumsPanel);
    addChildComponent (bassPanel);
    addChildComponent (pianoPanel);
    addChildComponent (guitarPanel);

    arrangementStrip = std::make_unique<BridgeArrangementStrip> (proc);
    addAndMakeVisible (*arrangementStrip);

    headerBar = std::make_unique<BridgeHeaderBar> (proc, [this] (int i) { activateTab (i); });
    addAndMakeVisible (*headerBar);

    mainPanel.onOpenTab = [this] (int tabIndex) { activateTab (tabIndex); };

    const int t = juce::jlimit (0, 4, proc.activeTab.load());
    proc.activeTab.store (t);

    // Must run after headerBar exists — early setSize skips header bounds and leaves a 0×0 header.
    setResizable (false, false);
    setSize (kWindowW, kWindowH);

    showTab (t);

    setWantsKeyboardFocus (true);

    startTimerHz (4);
}

BridgeEditor::~BridgeEditor()
{
    stopTimer();
}

bool BridgeEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (headerBar != nullptr && headerBar->bpmValueEditorHasKeyboardFocus())
            return false;

        if (auto* pr = proc.apvtsMain.getParameter ("transportPlaying"))
            pr->setValueNotifyingHost (pr->getValue() > 0.5f ? 0.0f : 1.0f);
        return true;
    }

    if (key.getModifiers().isAltDown())
    {
        const int k = key.getKeyCode();
        if (k >= '1' && k <= '5')
        {
            activateTab (k - '1');
            return true;
        }
    }

    return false;
}

void BridgeEditor::timerCallback()
{
    updateBpmDisplay();
}

void BridgeEditor::updateBpmDisplay()
{
    if (headerBar != nullptr)
        headerBar->syncBpmDisplay();
}

void BridgeEditor::activateTab (int index)
{
    index = juce::jlimit (0, 4, index);
    proc.activeTab.store (index);
    showTab (index);
}

void BridgeEditor::syncTabTileSelection()
{
    const int active = juce::jlimit (0, 4, proc.activeTab.load());
    if (headerBar != nullptr)
        headerBar->syncInstrumentTabSelection (active);
}

void BridgeEditor::bringHeaderToFront()
{
    if (headerBar != nullptr)
        headerBar->toFront (false);
}

void BridgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (bridge::colors::background());
}

void BridgeEditor::paintOverChildren (juce::Graphics&)
{
}

void BridgeEditor::resized()
{
    using namespace bridge::instrumentLayout;

    auto r = getLocalBounds();
    auto headerArea = r.removeFromTop (kHeaderH);

    if (headerBar != nullptr)
        headerBar->setBounds (headerArea);

    if (arrangementStrip != nullptr)
        arrangementStrip->setBounds (r.removeFromTop (kArrangementStripH));

    mainPanel.setBounds   (r);
    drumsPanel.setBounds (r);
    bassPanel.setBounds (r);
    pianoPanel.setBounds (r);
    guitarPanel.setBounds   (r);

    syncTabTileSelection();
    bringHeaderToFront();
}

void BridgeEditor::showTab (int index)
{
    index = juce::jlimit (0, 4, index);
    mainPanel.setVisible   (index == 0);
    drumsPanel.setVisible (index == 1);
    bassPanel.setVisible (index == 2);
    pianoPanel.setVisible (index == 3);
    guitarPanel.setVisible   (index == 4);

    syncTabTileSelection();
    bringHeaderToFront();
    repaint();
}

void BridgeEditor::notifyDrumsPatternChanged()
{
    drumsPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyBassPatternChanged()
{
    bassPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyPianoPatternChanged()
{
    pianoPanel.triggerAsyncUpdate();
}

void BridgeEditor::notifyGuitarPatternChanged()
{
    guitarPanel.triggerAsyncUpdate();
}
