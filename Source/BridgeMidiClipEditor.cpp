#include "BridgeMidiClipEditor.h"
#include "BridgeAppleHIG.h"
#include <algorithm>
#include "MelodicGridLayout.h"
#include "BridgeScaleHighlight.h"
#include "BridgePhrase.h"
#include "drums/DrumsStylePresets.h"
#include <cmath>

BridgeMidiClipEditor::BridgeMidiClipEditor (BridgeProcessor& processor,
                                            InstrumentKind instrument,
                                            juce::Colour accentColour,
                                            std::function<void (float)> onZoomTime,
                                            std::function<void (float)> onZoomPitch)
    : proc (processor),
      kind (instrument),
      accent (accentColour),
      zoomTime (std::move (onZoomTime)),
      zoomPitch (std::move (onZoomPitch))
{
    addAndMakeVisible (foldButton);
    foldButton.setClickingTogglesState (true);
    foldButton.onClick = [this] { repaint(); };
    foldButton.setColour (juce::ToggleButton::textColourId, bridge::hig::secondaryLabel);
    startTimerHz (24);
}

const BridgeClipTimeline& BridgeMidiClipEditor::clipRef() const
{
    switch (kind)
    {
        case InstrumentKind::drums:  return proc.getDrumsClipTimeline();
        case InstrumentKind::bass:   return proc.getBassClipTimeline();
        case InstrumentKind::piano:  return proc.getPianoClipTimeline();
        case InstrumentKind::guitar: return proc.getGuitarClipTimeline();
    }
    return proc.getPianoClipTimeline();
}

int BridgeMidiClipEditor::phraseSteps() const
{
    switch (kind)
    {
        case InstrumentKind::drums:   return proc.drumEngine.getPatternLen();
        case InstrumentKind::bass:    return proc.bassEngine.getPatternLen();
        case InstrumentKind::piano:   return proc.pianoEngine.getPatternLen();
        case InstrumentKind::guitar:  return proc.guitarEngine.getPatternLen();
    }
    return bridge::phrase::kStepsPerBar;
}

void BridgeMidiClipEditor::setZoom (float zx, float zy) noexcept
{
    zoomX = zx;
    zoomY = zy;
    repaint();
}

void BridgeMidiClipEditor::setVerticalScroll (int y) noexcept
{
    scrollY = juce::jmax (0, y);
    repaint();
}

void BridgeMidiClipEditor::updatePlayhead (int activeStep0)
{
    playheadStep = activeStep0;
    repaint();
}

void BridgeMidiClipEditor::layoutFoldButton()
{
    auto r = getLocalBounds().toFloat();
    foldButton.setBounds ((int) (r.getRight() - 72.0f), 2, 68, 18);
}

void BridgeMidiClipEditor::resized()
{
    layoutFoldButton();
}

void BridgeMidiClipEditor::paintTimeRuler (juce::Graphics& g, juce::Rectangle<float> rulerArea, float pxPerStep)
{
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (rulerArea);
    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.55f));
    g.drawLine (rulerArea.getX(), rulerArea.getBottom(), rulerArea.getRight(), rulerArea.getBottom(), 1.0f);

    const int n = phraseSteps();
    g.setFont (bridge::hig::uiFont (10.0f, "Semibold"));
    g.setColour (bridge::hig::secondaryLabel);
    const int barLen = bridge::phrase::kStepsPerBar;
    for (int s = 0; s <= n; s += barLen / 4) // quarter-bar ticks
    {
        const float x = rulerArea.getX() + (float) s * pxPerStep;
        if (x > rulerArea.getRight())
            break;
        g.drawVerticalLine ((int) x, rulerArea.getY(), rulerArea.getBottom());
        if (s % barLen == 0)
            g.drawText (juce::String (s / barLen + 1), juce::Rectangle<int> ((int) x + 2, (int) rulerArea.getY() + 1, 28, (int) rulerArea.getHeight() - 2),
                        juce::Justification::centredLeft, false);
    }
}

void BridgeMidiClipEditor::paintKeyStrip (juce::Graphics& g, juce::Rectangle<float> stripArea, float rowH,
                                          int rowLoMidi, int rowHiMidi, bool drumMode)
{
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (stripArea);
    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.55f));
    g.drawVerticalLine ((int) stripArea.getRight(), stripArea.getY(), stripArea.getBottom());

    const int nRows = juce::jmax (1, rowHiMidi - rowLoMidi + 1);
    const juce::String names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int r = 0; r < nRows; ++r)
    {
        const float y = stripArea.getBottom() - (float) (r + 1) * rowH;
        auto row = juce::Rectangle<float> (stripArea.getX(), y, stripArea.getWidth(), rowH);
        if (drumMode)
        {
            const int lane = (rowHiMidi - r); // row index encodes lane when drumMode rowLo=0 rowHi=NUM_DRUMS-1
            if (lane >= 0 && lane < NUM_DRUMS)
            {
                g.setColour (bridge::hig::secondaryLabel);
                g.setFont (bridge::hig::uiFont (10.0f));
                g.drawText (DRUM_NAMES[lane], row.reduced (2.0f, 0.0f), juce::Justification::centredLeft, true);
            }
        }
        else
        {
            const int midi = rowLoMidi + r;
            const bool bk = (midi % 12 == 1 || midi % 12 == 3 || midi % 12 == 6 || midi % 12 == 8 || midi % 12 == 10);
            juce::Colour fill = bk ? bridge::hig::systemBackground : bridge::hig::secondaryLabel;
            g.setColour (fill.withAlpha (0.55f));
            g.fillRect (row.reduced (0, 0.5f));
            g.setColour (bridge::hig::quaternaryLabel);
            g.setFont (bridge::hig::uiFont (10.0f));
            g.drawText (names[midi % 12] + juce::String (midi / 12 - 1),
                        row.reduced (2.0f, 0.0f), juce::Justification::centredRight, true);
        }
    }
}

void BridgeMidiClipEditor::paintNotes (juce::Graphics& g, juce::Rectangle<float> gridArea, float pxPerStep, float rowH,
                                       int rowLoMidi, int rowHiMidi, bool drumMode)
{
    g.setColour (bridge::hig::tertiaryGroupedBackground);
    g.fillRect (gridArea);

    const int nSteps = phraseSteps();
    const int tdIdx = bridge::phrase::readTimeDivisionChoiceIndex (proc.apvtsMain);
    const int accentPeriod = bridge::phrase::accentColumnPeriodInSixteenthsFromTimeDivisionIndex (tdIdx);
    for (int s = 0; s <= nSteps; ++s)
    {
        const float x = gridArea.getX() + (float) s * pxPerStep;
        const bool isAccentCol = (s % accentPeriod) == 0;
        g.setColour (isAccentCol ? bridge::hig::quaternaryFill : bridge::hig::separatorOpaque.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
    }

    const int nRows = juce::jmax (1, rowHiMidi - rowLoMidi + 1);
    for (int r = 0; r < nRows; ++r)
    {
        const float y = gridArea.getBottom() - (float) (r + 1) * rowH;
        g.setColour (bridge::hig::separatorOpaque.withAlpha (0.25f));
        g.drawHorizontalLine ((int) (y + rowH), gridArea.getX(), gridArea.getRight());
    }

    const double ppqPerStep = 0.25;
    const auto& clip = clipRef();

    for (const auto& n : clip.notes)
    {
        const float stepCenter = (float) n.stepIndex0 + (float) (n.microPpq / ppqPerStep);
        float x0 = gridArea.getX() + stepCenter * pxPerStep;
        float w  = juce::jmax (2.0f, (float) (n.lengthPpq / ppqPerStep) * pxPerStep);
        if (x0 + w > gridArea.getRight() + 40.0f || x0 < gridArea.getX() - 40.0f)
            continue;

        int rowFromBottom = -1;
        if (drumMode)
        {
            const int lane = n.drumLane();
            if (lane < 0 || lane >= NUM_DRUMS)
                continue;
            rowFromBottom = rowHiMidi - lane;
        }
        else
        {
            const int midi = (int) n.pitch;
            if (midi < rowLoMidi || midi > rowHiMidi)
                continue;
            rowFromBottom = midi - rowLoMidi;
        }

        const float y = gridArea.getBottom() - (float) (rowFromBottom + 1) * rowH;
        auto block = juce::Rectangle<float> (x0, y + rowH * 0.15f, w, rowH * 0.7f).reduced (0.5f, 0.0f);
        juce::Colour col = accent.withAlpha (n.isGhost() ? 0.38f : 0.88f);
        const float vel = n.velocity / 127.0f;
        col = col.withSaturation (juce::jlimit (0.15f, 1.0f, 0.35f + 0.65f * vel));
        g.setColour (col);
        g.fillRect (block);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRect (block, 0.8f);
    }

    if (playheadStep >= 0 && playheadStep < nSteps)
    {
        const float cx = gridArea.getX() + (float) playheadStep * pxPerStep;
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.drawVerticalLine ((int) cx, gridArea.getY(), gridArea.getBottom());
    }
}

void BridgeMidiClipEditor::paintVelocityLane (juce::Graphics& g, juce::Rectangle<float> laneArea, float pxPerStep)
{
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (laneArea, 3.0f);
    g.setColour (bridge::hig::separatorOpaque.withAlpha (0.45f));
    g.drawRoundedRectangle (laneArea.reduced (0.5f), 3.0f, 1.0f);

    g.setFont (bridge::hig::uiFont (9.0f));
    g.setColour (bridge::hig::quaternaryLabel);
    g.drawText ("127", laneArea.removeFromLeft (22).toNearestInt(), juce::Justification::centred, false);
    g.drawText ("1", laneArea.removeFromLeft (22).toNearestInt(), juce::Justification::centred, false);

    const auto& clip = clipRef();
    const double ppqPerStep = 0.25;
    for (const auto& n : clip.notes)
    {
        const float stepCenter = (float) n.stepIndex0 + (float) (n.microPpq / ppqPerStep);
        const float x = laneArea.getX() + stepCenter * pxPerStep;
        const float h = laneArea.getHeight() - 4.0f;
        const float vy = laneArea.getBottom() - 2.0f - (float) n.velocity / 127.0f * h;
        g.setColour (accent.withAlpha (0.85f));
        g.fillEllipse (x - 2.5f, vy - 2.5f, 5.0f, 5.0f);
        g.drawLine (x, vy, juce::jmin (x + 12.0f, laneArea.getRight() - 1.0f), vy, 1.0f);
    }
}

void BridgeMidiClipEditor::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    auto header = full.removeFromTop ((float) kRulerH);
    auto velo   = full.removeFromBottom ((float) kVelocityH);
    auto body   = full;

    const int strip = (int) bridge::kMelodicKeyStripWidth;
    auto ruler = header.withTrimmedLeft ((float) strip);
    auto foldR = header.removeFromRight (76.0f);
    juce::ignoreUnused (foldR);

    const int nSteps = juce::jmax (1, phraseSteps());
    const float gridW = juce::jmax (1.0f, body.getWidth() - (float) strip);
    const float basePx = gridW / (float) nSteps;
    const float pxPerStep = basePx * zoomX;

    bool drumMode = (kind == InstrumentKind::drums);
    int rowLo = 36, rowHi = 48;
    if (drumMode)
    {
        rowLo = 0;
        rowHi = NUM_DRUMS - 1;
        if (foldButton.getToggleState())
        {
            bool use[NUM_DRUMS] = {};
            for (const auto& n : clipRef().notes)
            {
                const int ln = n.drumLane();
                if (ln >= 0 && ln < NUM_DRUMS)
                    use[ln] = true;
            }
            int lo = NUM_DRUMS, hi = -1;
            for (int i = 0; i < NUM_DRUMS; ++i)
                if (use[i]) { lo = juce::jmin (lo, i); hi = juce::jmax (hi, i); }
            if (lo <= hi)
            {
                rowLo = lo;
                rowHi = hi;
            }
        }
    }
    else
    {
        switch (kind)
        {
            case InstrumentKind::bass:   bridge::applyRollSpanMelodicWindow (proc.bassEngine, rowLo, rowHi); break;
            case InstrumentKind::piano:  bridge::applyRollSpanMelodicWindow (proc.pianoEngine, rowLo, rowHi); break;
            case InstrumentKind::guitar: bridge::applyRollSpanMelodicWindow (proc.guitarEngine, rowLo, rowHi); break;
            case InstrumentKind::drums:  break;
        }
        if (foldButton.getToggleState())
        {
            juce::Array<int> used;
            clipRef().collectUsedPitches (used);
            if (used.size() > 0)
            {
                const int lo = used.getFirst();
                const int hi = used.getLast();
                rowLo = juce::jmax (rowLo, lo - 1);
                rowHi = juce::jmin (rowHi, hi + 1);
                if (rowLo > rowHi)
                    std::swap (rowLo, rowHi);
            }
        }
    }

    const int nRows = juce::jmax (1, rowHi - rowLo + 1);
    const float baseRowH = body.getHeight() / (float) nRows;
    const float rowH = baseRowH * zoomY;
    const int totalNoteH = (int) std::ceil ((float) nRows * rowH);
    const int maxScroll = juce::jmax (0, totalNoteH - (int) body.getHeight());
    scrollY = juce::jmin (scrollY, maxScroll);

    paintTimeRuler (g, ruler, pxPerStep);

    auto bodyShifted = body.translated (0.0f, (float) -scrollY);
    auto stripArea = bodyShifted.removeFromLeft ((float) strip);
    auto gridArea   = bodyShifted;

    paintKeyStrip (g, stripArea, rowH, rowLo, rowHi, drumMode);
    paintNotes (g, gridArea, pxPerStep, rowH, rowLo, rowHi, drumMode);

    auto vel = velo.withTrimmedLeft ((float) strip);
    paintVelocityLane (g, vel, pxPerStep);
}

void BridgeMidiClipEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        if (zoomTime)
            zoomTime (wheel.deltaY);
    }
    else if (e.mods.isAltDown())
    {
        if (zoomPitch)
            zoomPitch (wheel.deltaY);
    }
    else
    {
        scrollY += (int) std::round (wheel.deltaY * -40.0);
        scrollY = juce::jmax (0, scrollY);
        repaint();
    }
}

void BridgeMidiClipEditor::mouseMagnify (const juce::MouseEvent&, float scaleFactor)
{
    if (zoomPitch)
        zoomPitch (scaleFactor - 1.0f);
}

void BridgeMidiClipEditor::mouseDown (const juce::MouseEvent& e)
{
    const int strip = (int) bridge::kMelodicKeyStripWidth;
    if (e.position.x >= (float) strip)
        return;

    const bool drumMode = (kind == InstrumentKind::drums);
    if (drumMode)
    {
        int rowLo = 0, rowHi = NUM_DRUMS - 1;
        const int nRows = rowHi - rowLo + 1;
        const float bodyH = (float) (getHeight() - kRulerH - kVelocityH);
        const float rowH = (bodyH / (float) juce::jmax (1, nRows)) * zoomY;
        const int row = (int) ((e.position.y - (float) kRulerH + (float) scrollY) / juce::jmax (1.0f, rowH));
        const int lane = rowHi - row;
        if (lane >= 0 && lane < NUM_DRUMS)
        {
            const int note = juce::jlimit (1, 127, (int) DRUM_MIDI_NOTES[lane]);
            int ch = 10;
            if (proc.apvtsDrums.getRawParameterValue ("midiChannel") != nullptr)
                ch = (int) *proc.apvtsDrums.getRawParameterValue ("midiChannel");
            proc.queueMelodicPreviewNote (ch, note, 96);
        }
        return;
    }

    int rowLo = 60, rowHi = 72;
    switch (kind)
    {
        case InstrumentKind::bass:   bridge::applyRollSpanMelodicWindow (proc.bassEngine, rowLo, rowHi); break;
        case InstrumentKind::piano:  bridge::applyRollSpanMelodicWindow (proc.pianoEngine, rowLo, rowHi); break;
        case InstrumentKind::guitar: bridge::applyRollSpanMelodicWindow (proc.guitarEngine, rowLo, rowHi); break;
        case InstrumentKind::drums:  break;
    }
    const int nRows = juce::jmax (1, rowHi - rowLo + 1);
    const float bodyH = (float) (getHeight() - kRulerH - kVelocityH);
    const float rowH = (bodyH / (float) nRows) * zoomY;
    const int row = (int) ((e.position.y - (float) kRulerH + (float) scrollY) / juce::jmax (1.0f, rowH));
    const int midi = juce::jlimit (rowLo, rowHi, rowLo + row);
    int ch = 1;
    if (kind == InstrumentKind::bass && proc.apvtsBass.getRawParameterValue ("midiChannel") != nullptr)
        ch = (int) *proc.apvtsBass.getRawParameterValue ("midiChannel");
    else if (kind == InstrumentKind::piano && proc.apvtsPiano.getRawParameterValue ("midiChannel") != nullptr)
        ch = (int) *proc.apvtsPiano.getRawParameterValue ("midiChannel");
    else if (kind == InstrumentKind::guitar && proc.apvtsGuitar.getRawParameterValue ("midiChannel") != nullptr)
        ch = (int) *proc.apvtsGuitar.getRawParameterValue ("midiChannel");
    proc.queueMelodicPreviewNote (ch, midi, 96);
}
