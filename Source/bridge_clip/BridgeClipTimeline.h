#pragma once

#include "BridgeClipNote.h"
#include <JuceHeader.h>
#include <vector>

class DrumEngine;
class BassEngine;
class PianoEngine;
class GuitarEngine;

/** Clip notes for one instrument; rebuilt from engines and used for playback + piano roll UI. */
class BridgeClipTimeline
{
public:
    std::vector<BridgeClipNote> notes;

    void clear() noexcept { notes.clear(); }
    void sortByStepAndPitch();

    void toXml (juce::XmlElement& parent, const juce::String& tagName) const;
    /** Returns true if a `<tagName>` child element was present (even with zero notes). */
    bool fromXml (const juce::XmlElement& parent, const juce::String& tagName);

    void rebuildFromDrumEngine (DrumEngine& engine, int phraseSteps,
                                double samplesPerStep, double samplesPerBeat);
    void rebuildFromBassEngine (const BassEngine& engine, int phraseSteps,
                                double samplesPerStep, double samplesPerBeat);
    void rebuildFromPianoEngine (const PianoEngine& engine, int phraseSteps,
                                 double samplesPerStep, double samplesPerBeat);
    void rebuildFromGuitarEngine (const GuitarEngine& engine, int phraseSteps,
                                  double samplesPerStep, double samplesPerBeat);

    void applyIntoDrumEngine (DrumEngine& engine) const;
    void applyIntoBassEngine (BassEngine& engine) const;
    void applyIntoPianoEngine (PianoEngine& engine) const;
    void applyIntoGuitarEngine (GuitarEngine& engine) const;

    void collectUsedPitches (juce::Array<int>& outPitches) const;
};
