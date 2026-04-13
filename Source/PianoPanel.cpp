#include "PianoPanel.h"
#include "BridgePanelLayout.h"
#include "BridgePanelLayout.h"
#include "BridgeInstrumentStyles.h"
#include "MelodicGridLayout.h"

class FillHoldListener : public juce::MouseListener
{
public:
    FillHoldListener (BridgeProcessor& p, juce::TextButton& b) : proc (p), btn (b) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.pianoEngine.setFillHoldActive (true);
            proc.rebuildPianoGridPreview();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &btn)
        {
            proc.pianoEngine.setFillHoldActive (false);
            proc.rebuildPianoGridPreview();
        }
    }

private:
    BridgeProcessor& proc;
    juce::TextButton& btn;
};

// ─── PianoPianoRollComponent ───────────────────────────────────────────────────────

PianoPianoRollComponent::PianoPianoRollComponent (BridgeProcessor& p) : proc (p) {}

bool PianoPianoRollComponent::isBlackKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

void PianoPianoRollComponent::paint (juce::Graphics& g)
{
    using namespace PianoM3; // fallback
    auto full = getLocalBounds().toFloat();
    
    // Draw background for keyboard
    g.setColour (juce::Colour(0xff18141f));
    g.fillRoundedRectangle (full, 4.0f);
    
    auto& engine = proc.pianoEngine;
    int low = 60, high = 72;
    bridge::setOneOctaveMelodicRange (engine, low, high);
    int nRows = bridge::kMelodicOctaveRows;
    float rowH = full.getHeight() / (float) nRows;
    
    const juce::String names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    
    // Draw keys
    for (int m = high; m >= low; --m)
    {
        int idx = high - m;
        float y = full.getY() + (float) idx * rowH;
        auto row = juce::Rectangle<float> (full.getX(), y, full.getWidth(), rowH);

        bool bk = isBlackKey (m);
        // Modern piano roll keyboard look
        g.setColour (bk ? juce::Colour(0xff0c0a11) : juce::Colour(0xffe6e1d6));
        g.fillRect (row.reduced(0, 0.5f));

        g.setColour (juce::Colour(0xff2b2635));
        g.drawHorizontalLine ((int)(y + rowH), full.getX(), full.getRight());

        g.setColour (bk ? juce::Colour(0xff756e80) : juce::Colour(0xff110e16).withAlpha(0.85f));
        g.setFont (juce::Font(juce::FontOptions().withHeight (9.0f)));
        g.drawText (names[m % 12] + juce::String (m / 12 - 1),
                    row.reduced(2.0f, 0.0f).removeFromRight(full.getWidth() * 0.7f),
                    juce::Justification::centredRight, true);
    }
}

// ─── PianoGridComponent ────────────────────────────────────────────────────────

PianoGridComponent::PianoGridComponent (BridgeProcessor& p)
    : proc (p)
{}

void PianoGridComponent::paint (juce::Graphics& g)
{
    auto& engine  = proc.pianoEngine;
    auto& pattern = engine.getPatternForGrid();

    int loopStart = 1, loopEnd = BassPreset::NUM_STEPS;
    proc.getPianoLoopBounds (loopStart, loopEnd);
    const int ls0 = loopStart - 1;
    const int le0 = loopEnd - 1;

    auto full = getLocalBounds().toFloat();
    
    // Solid deep background for piano roll
    g.setColour (juce::Colour(0xff18141f));
    g.fillAll();

    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;
    const int nSteps = BassPreset::NUM_STEPS;

    float cellW = full.getWidth() / (float)nSteps;
    float cellH = full.getHeight() / (float)nRows;

    // Draw grid lines
    g.setColour (juce::Colour(0xff3a3548).withAlpha(0.4f));
    for (int row = 0; row < nRows; ++row) {
        float cy = (float) row * cellH;
        g.drawHorizontalLine ((int)cy, 0.0f, full.getWidth());
    }
    for (int step = 0; step < nSteps; ++step) {
        float cx = (float) step * cellW;
        bool isBeat = (step % 4 == 0);
        g.setColour (isBeat ? juce::Colour(0xff4a4558) : juce::Colour(0xff2b2635).withAlpha(0.6f));
        g.drawVerticalLine ((int)cx, 0.0f, full.getHeight());
    }

    // Dim out-of-loop areas
    if (ls0 > 0) {
        g.setColour (juce::Colours::black.withAlpha(0.4f));
        g.fillRect (0.0f, 0.0f, ls0 * cellW, full.getHeight());
    }
    if (le0 < nSteps - 1) {
        g.setColour (juce::Colours::black.withAlpha(0.4f));
        g.fillRect ((le0 + 1) * cellW, 0.0f, (nSteps - le0 - 1) * cellW, full.getHeight());
    }

    // Draw Active Notes (Capsules)
    juce::Colour accent = bridge::colors::accentPiano;
    for (int step = 0; step < nSteps; ++step)
    {
        const auto& hit = pattern[(size_t) step];
        if (! hit.active) continue;

        const int midi = juce::jlimit (minMidi, maxMidi, hit.midiNote);
        int displayRow = maxMidi - midi;
        displayRow = juce::jlimit (0, nRows - 1, displayRow);

        float cx = (float) step * cellW;
        float cy = (float) displayRow * cellH;
        auto cell = juce::Rectangle<float> (cx, cy, cellW, cellH);

        juce::Colour col = accent.withAlpha (hit.isGhost ? 0.35f : 0.85f);
        float velFrac = hit.velocity / 127.0f;
        if (hit.isGhost)
            col = col.withAlpha (0.4f + velFrac * 0.2f);
        if (step == currentStep)
            col = col.brighter (0.4f);

        // Natively span the cell width. We reduce Y to make them pill-like.
        auto block = cell.reduced(1.0f, cellH * 0.2f);
        
        g.setColour (col);
        g.fillRoundedRectangle (block, 3.0f);
    }

    // Playhead
    if (currentStep >= 0 && currentStep < nSteps)
    {
        float cx = (float) currentStep * cellW;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect  (cx, 0.0f, cellW, full.getHeight());
        
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect  (cx, 0.0f, 2.0f, full.getHeight());
    }
}

void PianoGridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;
    const int nSteps = BassPreset::NUM_STEPS;
    float cellW = full.getWidth() / (float)nSteps;
    float cellH = full.getHeight() / (float)nRows;

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;
    
    dragOriginStep = step;
    
    // Allow standard left click to create/move
    auto& pat = const_cast<PianoPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;
    
    if (! pat[(size_t)step].active || pat[(size_t)step].midiNote != targetMidi)
    {
        const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
        pat[(size_t)step].active   = true;
        pat[(size_t)step].degree   = deg;
        pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
        pat[(size_t)step].velocity = 100;
        pat[(size_t)step].isGhost  = false;
    }
    proc.rebuildPianoGridPreview();
    repaint();
}


void PianoGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragOriginStep < 0) return;
    
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    int minMidi = 60, maxMidi = 72;
    bridge::setOneOctaveMelodicRange (engine, minMidi, maxMidi);
    const int nRows = bridge::kMelodicOctaveRows;
    const int nSteps = BassPreset::NUM_STEPS;
    float cellW = full.getWidth() / (float)nSteps;
    float cellH = full.getHeight() / (float)nRows;

    int step = (int)(e.x / cellW);
    int row  = (int)(e.y / cellH);
    if (step < 0 || step >= nSteps || row < 0 || row >= nRows) return;

    if (step != dragOriginStep)
    {
        auto& pat = const_cast<PianoPattern&>(engine.getPattern());
        // Move note securely
        if (pat[(size_t)dragOriginStep].active)
        {
            pat[(size_t)step] = pat[(size_t)dragOriginStep];
            pat[(size_t)dragOriginStep].active = false;
        }
        dragOriginStep = step; // update anchor
    }
    
    // Update pitch if row changed
    auto& pat = const_cast<PianoPattern&>(engine.getPattern());
    const int targetMidi = juce::jlimit (minMidi, maxMidi, maxMidi - row);
    int prevMidi = step > 0 ? pat[(size_t) (step - 1)].midiNote : -1;
    
    if (pat[(size_t)step].active && pat[(size_t)step].midiNote != targetMidi)
    {
        const int deg = engine.nearestDegreeForMidi (targetMidi, prevMidi);
        pat[(size_t)step].degree   = deg;
        pat[(size_t)step].midiNote = engine.degreeToMidiNote (deg, prevMidi);
    }

    proc.rebuildPianoGridPreview();
    repaint();
}

void PianoGridComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& engine = proc.pianoEngine;
    auto full = getLocalBounds().toFloat();
    const int nSteps = BassPreset::NUM_STEPS;
    float cellW = full.getWidth() / (float)nSteps;
    int step = (int)(e.x / cellW);
    
    if (step >= 0 && step < nSteps)
    {
        auto& pat = const_cast<PianoPattern&>(engine.getPattern());
        pat[(size_t)step].active = false;
        proc.rebuildPianoGridPreview();
        repaint();
    }
}
void PianoGridComponent::resized() {}

void PianoGridComponent::update (int activeStep)
{
    currentStep = activeStep;
    repaint();
}

// ─── Refactored ────────────────────────────────────────────────────────────

PianoPanel::PianoPanel (BridgeProcessor& p)
    : proc (p),
      bottomHalf (p.apvtsPiano, laf, bridge::colors::accentPiano,
        [this] { proc.triggerPianoGenerate(); },
        [this] (bool active) { proc.pianoEngine.setFillHoldActive (active); })
{
    setLookAndFeel (&laf);

    proc.apvtsPiano.addParameterListener ("loopStart", this);
    proc.apvtsPiano.addParameterListener ("loopEnd", this);
    proc.apvtsPiano.addParameterListener ("loopOn", this);
    proc.apvtsPiano.addParameterListener ("tickerSpeed", this);
    proc.apvtsPiano.addParameterListener ("style", this);
    proc.apvtsPiano.state.addListener (this);

    addAndMakeVisible (pianoRoll);
    addAndMakeVisible (grid);
    addAndMakeVisible (bottomHalf);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setColour (juce::Label::textColourId, bridge::colors::textDim);
    styleLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (styleLabel);

    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleBox.addItem (bridgeUnifiedStyleNames()[i], i + 1);
    addAndMakeVisible (styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvtsPiano, "style", styleBox);
    auto setupCombo = [&](juce::Label& lbl, juce::ComboBox& box, const juce::String& title, const juce::String& paramId) {
        lbl.setText (title, juce::dontSendNotification);
        lbl.setColour (juce::Label::textColourId, bridge::colors::textDim);
        lbl.setFont (juce::Font(juce::FontOptions().withHeight(12.0f)));
        addAndMakeVisible (lbl);
        addAndMakeVisible (box);
        if (proc.apvtsPiano.getParameter(paramId)) {
            if (title == "ROOT") rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvtsPiano, paramId, box);
            else if (title == "SCALE") scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvtsPiano, paramId, box);
            else if (title == "OCT") octaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvtsPiano, paramId, box);
        }
    };

    setupCombo(rootLabel, rootBox, "ROOT", "rootNote");
    static const char* roots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) rootBox.addItem(roots[i], i + 1);

    setupCombo(scaleLabel, scaleBox, "SCALE", "scale");
    for (int i = 0; i < BassPreset::NUM_SCALES; ++i) scaleBox.addItem(BassPreset::SCALE_NAMES[i], i + 1);

    setupCombo(octaveLabel, octaveBox, "OCT", "octave");
    for (int i = 1; i <= 4; ++i) octaveBox.addItem(juce::String(i), i);


    
    proc.rebuildPianoGridPreview();
    stepTimer.startTimerHz (30);
}

PianoPanel::~PianoPanel()
{
}

void PianoPanel::setLoopIntParameter (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& id, int value)
{
    value = jlimit (1, PianoPreset::NUM_STEPS, value);
    if (auto* p = apvts.getParameter (id))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) value));
}

void PianoPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    proc.rebuildPianoGridPreview();
    triggerAsyncUpdate();
}

namespace
{
    void layoutPianoDropdownRow (juce::Rectangle<int> row,
                                  juce::Label& styleLab, juce::ComboBox& styleBox,
                                  juce::Label& rootLab, juce::ComboBox& rootBox,
                                  juce::Label& scaleLab, juce::ComboBox& scaleBox,
                                  juce::Label& octLab, juce::ComboBox& octBox)
    {
        const int h = bridge::instrumentLayout::kDropdownH;
        const int labY = row.getCentreY() - (int) (h * 0.5f);

        auto placeGroup = [&] (juce::Label& lab, int lw, juce::ComboBox& box, int bw) {
            lab.setBounds (row.removeFromLeft (lw).withHeight (h).withY (labY)); row.removeFromLeft (4);
            box.setBounds (row.removeFromLeft (bw).withHeight (h).withY (labY)); row.removeFromLeft (14);
        };

        placeGroup (styleLab, 42, styleBox, 164);
        row.removeFromLeft(30); // spacer
        placeGroup (rootLab,  40, rootBox,  52);
        placeGroup (scaleLab, 45, scaleBox, 140);
        placeGroup (octLab,   35, octBox,   52);
    }
}

void PianoPanel::resized()
{
    using namespace bridge::instrumentLayout;

    const auto inner = getLocalBounds().reduced (16);
    auto shell = bridge::panelLayout::splitInstrumentContent (inner, 42);

    auto dropdownRow = inner.withHeight (42);
    layoutPianoDropdownRow (dropdownRow, styleLabel, styleBox, rootLabel, rootBox, scaleLabel, scaleBox, octaveLabel, octaveBox);

    auto card = shell.mainCard.reduced (8, 8);
    pianoRoll.setBounds (card.removeFromLeft (64));
    grid.setBounds (card);

    bottomHalf.setBounds (shell.knobsCard.getUnion (shell.loopActionsCard));
}

void PianoPanel::paint (juce::Graphics& g)
{
    using namespace bridge::instrumentLayout;
    g.fillAll (bridge::colors::background);

    auto shell = bridge::panelLayout::splitInstrumentContent(getLocalBounds().reduced(16), 42);
    
    auto drawCard = [&] (juce::Rectangle<int> r)
    {
        auto rf = r.toFloat();
        g.setColour (bridge::colors::cardSurface);
        g.fillRoundedRectangle (rf, (float) kCardRadius);
        g.setColour (bridge::colors::cardOutline.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), (float) kCardRadius, 1.0f);
    };
    drawCard (shell.mainCard);
}

void PianoPanel::handleAsyncUpdate()
{
    pianoRoll.repaint();
    grid.repaint();
}

void PianoPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);
    proc.rebuildPianoGridPreview();
    triggerAsyncUpdate();
}

void PianoPanel::updateStepAnimation()
{
    const int step = proc.pianoCurrentVisualStep.load();
    if (step != lastAnimStep)
    {
        lastAnimStep = step;
        grid.update (step);
    }
}
