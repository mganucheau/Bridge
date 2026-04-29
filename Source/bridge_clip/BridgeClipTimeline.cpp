#include "BridgeClipTimeline.h"
#include "../drums/DrumEngine.h"
#include "../bass/BassEngine.h"
#include "../piano/PianoEngine.h"
#include "../guitar/GuitarEngine.h"
#include "../drums/DrumsStylePresets.h"
#include <algorithm>

namespace
{
static double microPpqFromSamples (int swingSamples, int humanSamples, double samplesPerBeat)
{
    if (samplesPerBeat <= 1.0e-9)
        return 0.0;
    return ((double) swingSamples + (double) humanSamples) / samplesPerBeat;
}
} // namespace

void BridgeClipTimeline::sortByStepAndPitch()
{
    std::sort (notes.begin(), notes.end(), [] (const BridgeClipNote& a, const BridgeClipNote& b)
    {
        if (a.stepIndex0 != b.stepIndex0)
            return a.stepIndex0 < b.stepIndex0;
        if (a.pitch != b.pitch)
            return a.pitch < b.pitch;
        return a.microPpq < b.microPpq;
    });
}

void BridgeClipTimeline::toXml (juce::XmlElement& parent, const juce::String& tagName) const
{
    auto* clip = parent.createNewChildElement (tagName);
    for (const auto& n : notes)
    {
        auto* el = clip->createNewChildElement ("n");
        el->setAttribute ("s", n.stepIndex0);
        el->setAttribute ("m", n.microPpq);
        el->setAttribute ("p", (int) n.pitch);
        el->setAttribute ("v", (int) n.velocity);
        el->setAttribute ("l", n.lengthPpq);
        el->setAttribute ("f", (int) n.flags);
    }
}

bool BridgeClipTimeline::fromXml (const juce::XmlElement& parent, const juce::String& tagName)
{
    clear();
    auto* clip = parent.getChildByName (tagName);
    if (clip == nullptr)
        return false;

    for (auto* el : clip->getChildIterator())
    {
        if (! el->hasTagName ("n"))
            continue;
        BridgeClipNote n;
        n.stepIndex0 = el->getIntAttribute ("s", 0);
        n.microPpq   = el->getDoubleAttribute ("m", 0.0);
        n.pitch      = (uint8_t) juce::jlimit (0, 127, el->getIntAttribute ("p", 60));
        n.velocity   = (uint8_t) juce::jlimit (1, 127, el->getIntAttribute ("v", 100));
        n.lengthPpq  = juce::jmax (0.001, el->getDoubleAttribute ("l", 0.25));
        n.flags      = (uint8_t) juce::jlimit (0, 255, el->getIntAttribute ("f", 0));
        notes.push_back (n);
    }
    sortByStepAndPitch();
    return true;
}

void BridgeClipTimeline::rebuildFromDrumEngine (DrumEngine& engine, int phraseSteps,
                                                double samplesPerStep, double samplesPerBeat)
{
    clear();
    const int plen = juce::jmin (phraseSteps, juce::jmax (1, engine.getPatternLen()));
    for (int s = 0; s < plen; ++s)
    {
        DrumStep hits;
        engine.evaluateStepForPlayback (s, s, hits, samplesPerStep);
        const int swingOff = engine.getSwingOffset (s, samplesPerStep);
        for (int d = 0; d < NUM_DRUMS; ++d)
        {
            if (! hits[(size_t) d].active)
                continue;
            BridgeClipNote n;
            n.stepIndex0 = s;
            n.microPpq   = microPpqFromSamples (swingOff, hits[(size_t) d].timeShift, samplesPerBeat);
            n.pitch      = (uint8_t) juce::jlimit (1, 127, (int) DRUM_MIDI_NOTES[d]);
            n.velocity   = hits[(size_t) d].velocity;
            const int64 gateSamples = juce::jmax ((int64) 64,
                                                    (int64) (samplesPerStep * 0.18));
            n.lengthPpq  = (double) gateSamples / samplesPerBeat;
            n.setGhost (false);
            n.setDrumLane (d);
            notes.push_back (n);
        }
    }
    sortByStepAndPitch();
}

void BridgeClipTimeline::rebuildFromBassEngine (const BassEngine& engine, int phraseSteps,
                                                double samplesPerStep, double samplesPerBeat)
{
    clear();
    const auto& pat = engine.getPatternForGrid();
    const int plen = juce::jmin (phraseSteps, juce::jmax (1, engine.getPatternLen()));
    for (int s = 0; s < plen && s < (int) pat.size(); ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        BridgeClipNote n;
        n.stepIndex0 = s;
        const int swingOff = engine.getSwingOffset (s, samplesPerStep);
        n.microPpq   = microPpqFromSamples (swingOff, h.timeShift, samplesPerBeat);
        n.pitch      = (uint8_t) juce::jlimit (0, 127, h.midiNote);
        n.velocity   = h.velocity;
        const int dur = engine.calcNoteDuration (h, samplesPerStep);
        n.lengthPpq  = juce::jmax (0.001, (double) dur / samplesPerBeat);
        n.setGhost (h.isGhost);
        n.setDrumLane (-1);
        notes.push_back (n);
    }
    sortByStepAndPitch();
}

void BridgeClipTimeline::rebuildFromPianoEngine (const PianoEngine& engine, int phraseSteps,
                                                 double samplesPerStep, double samplesPerBeat)
{
    clear();
    const auto& pat = engine.getPatternForGrid();
    const int plen = juce::jmin (phraseSteps, juce::jmax (1, engine.getPatternLen()));
    for (int s = 0; s < plen && s < (int) pat.size(); ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        BridgeClipNote n;
        n.stepIndex0 = s;
        const int swingOff = engine.getSwingOffset (s, samplesPerStep);
        n.microPpq   = microPpqFromSamples (swingOff, h.timeShift, samplesPerBeat);
        n.pitch      = (uint8_t) juce::jlimit (0, 127, h.midiNote);
        n.velocity   = h.velocity;
        const int dur = engine.calcNoteDuration (h, samplesPerStep);
        n.lengthPpq  = juce::jmax (0.001, (double) dur / samplesPerBeat);
        n.setGhost (h.isGhost);
        n.setDrumLane (-1);
        notes.push_back (n);
    }
    sortByStepAndPitch();
}

void BridgeClipTimeline::rebuildFromGuitarEngine (const GuitarEngine& engine, int phraseSteps,
                                                  double samplesPerStep, double samplesPerBeat)
{
    clear();
    const auto& pat = engine.getPatternForGrid();
    const int plen = juce::jmin (phraseSteps, juce::jmax (1, engine.getPatternLen()));
    for (int s = 0; s < plen && s < (int) pat.size(); ++s)
    {
        const auto& h = pat[(size_t) s];
        if (! h.active)
            continue;
        BridgeClipNote n;
        n.stepIndex0 = s;
        const int swingOff = engine.getSwingOffset (s, samplesPerStep);
        n.microPpq   = microPpqFromSamples (swingOff, h.timeShift, samplesPerBeat);
        n.pitch      = (uint8_t) juce::jlimit (0, 127, h.midiNote);
        n.velocity   = h.velocity;
        const int dur = engine.calcNoteDuration (h, samplesPerStep);
        n.lengthPpq  = juce::jmax (0.001, (double) dur / samplesPerBeat);
        n.setGhost (h.isGhost);
        n.setDrumLane (-1);
        notes.push_back (n);
    }
    sortByStepAndPitch();
}

void BridgeClipTimeline::applyIntoDrumEngine (DrumEngine& e) const
{
    int patternLen = juce::jmax (1, e.getPatternLen());
    auto& pat = e.editPattern();
    if ((int) pat.size() < patternLen)
        pat.resize ((size_t) patternLen);
    for (int s = 0; s < patternLen && s < (int) pat.size(); ++s)
        for (int d = 0; d < NUM_DRUMS; ++d)
        {
            pat[(size_t) s][(size_t) d].active   = false;
            pat[(size_t) s][(size_t) d].velocity = 100;
            pat[(size_t) s][(size_t) d].timeShift = 0;
        }

    for (const auto& n : notes)
    {
        if (n.stepIndex0 < 0 || n.stepIndex0 >= patternLen)
            continue;
        const int lane = n.drumLane();
        if (lane < 0 || lane >= NUM_DRUMS)
            continue;
        auto& h = pat[(size_t) n.stepIndex0][(size_t) lane];
        h.active    = true;
        h.velocity  = n.velocity;
        h.timeShift = 0;
    }
}

void BridgeClipTimeline::applyIntoBassEngine (BassEngine& e) const
{
    if (e.isLocked())
        return;
    int patternLen = juce::jmax (1, e.getPatternLen());
    auto& pat = const_cast<BassPattern&> (e.getPattern());
    if ((int) pat.size() < patternLen)
        pat.resize ((size_t) patternLen);
    for (int s = 0; s < patternLen && s < (int) pat.size(); ++s)
        pat[(size_t) s].active = false;

    for (const auto& n : notes)
    {
        if (n.stepIndex0 < 0 || n.stepIndex0 >= patternLen)
            continue;
        auto& h = pat[(size_t) n.stepIndex0];
        h.active    = true;
        h.midiNote  = (int) n.pitch;
        h.velocity  = n.velocity;
        h.isGhost   = n.isGhost();
        h.degree    = 0;
        h.timeShift = 0;
    }
}

void BridgeClipTimeline::applyIntoPianoEngine (PianoEngine& e) const
{
    if (e.isLocked())
        return;
    int patternLen = juce::jmax (1, e.getPatternLen());
    auto& pat = const_cast<PianoPattern&> (e.getPattern());
    if ((int) pat.size() < patternLen)
        pat.resize ((size_t) patternLen);
    for (int s = 0; s < patternLen && s < (int) pat.size(); ++s)
        pat[(size_t) s].active = false;

    for (const auto& n : notes)
    {
        if (n.stepIndex0 < 0 || n.stepIndex0 >= patternLen)
            continue;
        auto& h = pat[(size_t) n.stepIndex0];
        h.active    = true;
        h.midiNote  = (int) n.pitch;
        h.velocity  = n.velocity;
        h.isGhost   = n.isGhost();
        h.degree    = 0;
        h.timeShift = 0;
    }
}

void BridgeClipTimeline::applyIntoGuitarEngine (GuitarEngine& e) const
{
    if (e.isLocked())
        return;
    int patternLen = juce::jmax (1, e.getPatternLen());
    auto& pat = const_cast<GuitarPattern&> (e.getPattern());
    if ((int) pat.size() < patternLen)
        pat.resize ((size_t) patternLen);
    for (int s = 0; s < patternLen && s < (int) pat.size(); ++s)
        pat[(size_t) s].active = false;

    for (const auto& n : notes)
    {
        if (n.stepIndex0 < 0 || n.stepIndex0 >= patternLen)
            continue;
        auto& h = pat[(size_t) n.stepIndex0];
        h.active    = true;
        h.midiNote  = (int) n.pitch;
        h.velocity  = n.velocity;
        h.isGhost   = n.isGhost();
        h.degree    = 0;
        h.timeShift = 0;
    }
}

void BridgeClipTimeline::collectUsedPitches (juce::Array<int>& outPitches) const
{
    outPitches.clearQuick();
    for (const auto& n : notes)
    {
        const int p = (int) n.pitch;
        if (! outPitches.contains (p))
            outPitches.add (p);
    }
    outPitches.sort();
}
