#pragma once

#include <JuceHeader.h>
#include "GuitarStylePresets.h"
#include <array>
#include <random>
#include <functional>
#include <vector>

class BridgeMLManager;

// ─────────────────────────────────────────────────────────────────────────────
// GuitarHit — one 16th-note slot in the generated bass line
// ─────────────────────────────────────────────────────────────────────────────
struct GuitarHit
{
    bool    active      = false;
    uint8   velocity    = 100;
    int     midiNote    = 36;     // absolute MIDI note (root + degree offset)
    int     degree      = 0;      // degree index (0–7) for UI display
    bool    isGhost     = false;  // affects velocity tier and note duration
    int     timeShift   = 0;      // timing feel + humanize, in samples
    int     durationSamples = 0;  // calculated at scheduling time
};

using GuitarPattern = std::array<GuitarHit, GuitarPreset::NUM_STEPS>;

// ─────────────────────────────────────────────────────────────────────────────
class GuitarEngine
{
public:
    GuitarEngine();

    // ── Generation ─────────────────────────────────────────────────────────
    void generatePattern (bool seamlessPerform = false, BridgeMLManager* ml = nullptr);
    void generatePatternRange (int fromStep0, int toStep0, bool seamlessPerform = false, BridgeMLManager* ml = nullptr);
    void generateFill (int fromStep = 12);

    // ── Pattern access ─────────────────────────────────────────────────────
    const GuitarHit&     getStep    (int step) const { return pattern[(size_t)step]; }
    const GuitarPattern& getPattern ()         const { return pattern; }
    const GuitarPattern& getPatternForGrid()   const { return displayPattern; }
    bool               isLocked  ()          const { return locked; }

    void setFillHoldActive (bool on) { fillHoldActive = on; }
    void rebuildGridPreview();
    void morphPatternForDensityAndComplexity();
    void adaptPatternToNewStyle (int newStyleIndex);
    void evolvePatternRangeForJam (int fromStep0, int toStep0, BridgeMLManager* ml);

    // ── Parameters ─────────────────────────────────────────────────────────
    void setStyle       (int  s) { style       = jlimit (0, GuitarPreset::NUM_STYLES - 1, s); }
    void setScale       (int  s) { scale       = jlimit (0, GuitarPreset::NUM_SCALES - 1, s); }
    void setRootNote    (int  r) { rootNote    = jlimit (0, 11, r); }  // 0=C … 11=B
    void setOctave      (int  o) { octave      = jlimit (1, 4, o);  }  // MIDI octave (2=bass register)
    void setTemperature (float t){ temperature = jlimit (0.01f, 2.0f,  t); }
    void setDensity     (float d){ density     = jlimit (0.0f,  1.0f,  d); }
    void setSwing       (float s){ swing       = jlimit (0.0f,  1.0f,  s); }
    void setHumanize    (float h){ humanize    = jlimit (0.0f,  1.0f,  h); }
    void setHold        (float h){ hold        = jlimit (0.0f,  1.0f,  h); }  // note length + residual timing feel
    void setVelocity    (float v){ velocityMul = jlimit (0.0f,  1.0f,  v); }
    void setFillRate    (float f){ fillRate    = jlimit (0.0f,  1.0f,  f); }
    void setComplexity  (float c){ complexity  = jlimit (0.0f,  1.0f,  c); }
    void setGhostAmount (float g){ ghostAmount = jlimit (0.0f,  1.0f,  g); }  // scales ghost tendency
    void setStaccato    (float s){ staccato    = jlimit (0.0f,  1.0f,  s); }  // 0=legato, 1=staccato
    void setPatternLen  (int   l){ patternLen  = jlimit (1, GuitarPreset::NUM_STEPS, l); }
    void setLocked      (bool  l){ locked      = l; }
    void setSeed        (uint32 s){ seed       = s; rng.seed (seed); }
    void setPhraseBars  (int bars){ phraseBars = jlimit (1, 64, bars); }

    int   getStyle      () const { return style; }
    int   getScale      () const { return scale; }
    int   getRootNote   () const { return rootNote; }
    int   getOctave     () const { return octave; }
    float getTemperature() const { return temperature; }
    float getDensity    () const { return density; }
    float getSwing      () const { return swing; }
    float getHumanize   () const { return humanize; }
    float getHold       () const { return hold; }
    float getVelocity   () const { return velocityMul; }
    float getFillRate   () const { return fillRate; }
    float getComplexity () const { return complexity; }
    float getGhostAmount() const { return ghostAmount; }
    float getStaccato   () const { return staccato; }
    int   getPatternLen () const { return patternLen; }
    uint32 getSeed      () const { return seed; }
    int   getPhraseBars () const { return phraseBars; }

    int  getRootMidiBase () const { return rootMidiBase(); }

    void setHostSampleRate (double sr) { hostSampleRate = juce::jmax (1.0, sr); }

    void remapPatternAfterTonalityChange (int previousRootMidiBase, int previousScaleIndex);

    void setMLPersonalityKnobs (const std::array<float, 10>& k) { mlPersonalityKnobs = k; }
    void captureMLContext();
    void mergePatternFromML (const std::vector<float>& mlOutput);

    // Swing offset in samples for a given step
    int  getSwingOffset (int step, double samplesPerStep) const;

    // Combined timing feel offset (scaled by hold) in samples
    int  getTimingFeelOffset (int step, double samplesPerStep) const;

    // Note duration in samples for a given hit
    int  calcNoteDuration (const GuitarHit& hit, double samplesPerStep) const;

    // Called each bar — may auto-trigger a fill based on fillRate
    bool shouldTriggerFill();

    // Register callback so editor can repaint when pattern changes
    std::function<void()> onPatternChanged;

    // Resolve absolute MIDI note from degree, style, root, octave, scale
    int  degreeToMidiNote (int degree, int prevMidi = -1) const;
    int  nearestDegreeForMidi (int midi, int prevMidi = -1) const;

private:
    GuitarPattern  pattern;
    GuitarPattern  displayPattern {};  // pattern + optional fill-hold overlay for UI
    std::mt19937 rng;
    bool         fillHoldActive = false;

    int   style       = 0;
    int   scale       = 0;   // 0=Dorian
    int   rootNote    = 0;   // 0=C
    int   octave      = 3;   // guitar range (~C3)
    float temperature = 1.0f;
    float density     = 0.70f;
    float swing       = 0.0f;
    float humanize    = 0.20f;
    float hold        = 0.50f;
    float velocityMul = 0.85f;
    float fillRate    = 0.15f;
    float complexity  = 0.50f;
    float ghostAmount = 0.70f;
    float staccato    = 0.20f;
    int   patternLen  = GuitarPreset::NUM_STEPS;
    bool  locked      = false;
    uint32 seed       = 42;
    int   barCount    = 0;
    int   phraseBars  = 4;

    int   rootMidiBase() const;            // = rootNote + (octave + 1) * 12
    int   snapMidiToCurrentScale (int midi) const;
    float sampleProb (float p) const;
    uint8 sampleVelocity (int step, bool ghost, bool accent) const;
    int   chooseDegreeProbabilistic (int step, int preferredDegree) const;
    int   resolveApproachNote (int step) const;  // looks ahead for chromatic approach
    void  resolveApproachNotes();                 // post-pass: fix approach degrees
    float complexityMod (int step) const;
    void  applyHumanize (double samplesPerStep);
    void  mergeStrumFromML (BridgeMLManager* ml);
    void  applyGuitarInertiaML (BridgeMLManager* ml);

    std::array<float, 10> mlPersonalityKnobs {};
    std::array<float, 16> mlNoteContext {};
    std::array<float, 16> mlRhythmicGrid {};

    double hostSampleRate = 44100.0;
};
