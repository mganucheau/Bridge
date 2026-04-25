#pragma once

#include <JuceHeader.h>
#include "DrumsStylePresets.h"
#include <array>
#include <random>
#include <functional>
#include <vector>

class BridgeMLManager;

struct DrumHit
{
    bool  active    = false;
    uint8 velocity  = 100;
    int   timeShift = 0;   // in samples, for humanization
};

using DrumStep   = std::array<DrumHit, NUM_DRUMS>;
using DrumPattern = std::array<DrumStep, NUM_STEPS>;

// ─────────────────────────────────────────────────────────────────────────────
class DrumEngine
{
public:
    DrumEngine();

    // ── Generation ─────────────────────────────────────────────────────────
    /** Sting-style per-layer locks: when a layer is locked, generate copies that layer from the
        previous pattern instead of regenerating it. Layer 0 = kick, 1 = snare, 2 = hats, 3 = perc. */
    struct LayerMask
    {
        bool kick  = false;
        bool snare = false;
        bool hats  = false;
        bool perc  = false;

        bool any() const noexcept { return kick || snare || hats || perc; }
    };

    void generatePattern (bool seamlessPerform = false, BridgeMLManager* ml = nullptr);
    /** Regenerate only steps from0..to0 inclusive (pattern indices 0..NUM_STEPS-1). */
    void generatePatternRange (int from0, int to0, bool seamlessPerform = false, BridgeMLManager* ml = nullptr);
    void generateFill (int fromStep = 12);  // from step 12 = last 4 steps

    /** Set per-layer lock state used by the next generate*. */
    void setLayerLocks (LayerMask m) noexcept { layerLocks = m; }
    LayerMask getLayerLocks() const noexcept { return layerLocks; }

    // ── Pattern access ─────────────────────────────────────────────────────
    const DrumStep& getStep (int step) const
    {
        return pattern[(size_t) juce::jlimit (0, NUM_STEPS - 1, step)];
    }
    const DrumPattern& getPattern()       const { return pattern; }
    const DrumPattern& getPatternForGrid() const { return gridPreview; }

    void rebuildGridPreview();
    void morphPatternForDensityAndComplexity (int rangeFromStep0 = -1, int rangeToStep0 = -1);
    void adaptPatternToNewStyle (int newStyleIndex);
    void evolvePatternRangeForJam (int fromStep0, int toStep0, BridgeMLManager* ml);

    /** Editor / UI: mutate the committed pattern (then call rebuildGridPreview). */
    DrumPattern& editPattern() noexcept { return pattern; }

    // ── Parameters (0.0–1.0 unless noted) ─────────────────────────────────
    void setStyle      (int s)   { style      = jlimit(0, NUM_STYLES - 1, s); }
    void setTemperature(float t) { temperature = jlimit(0.01f, 2.0f,  t); }
    void setDensity    (float d) { density     = jlimit(0.0f,  1.0f,  d); }
    void setSwing      (float s) { swing       = jlimit(0.0f,  1.0f,  s); }
    void setHumanize   (float h) { humanize    = jlimit(0.0f,  1.0f,  h); }
    void setVelocity   (float v) { velocityMul = jlimit(0.0f,  1.0f,  v); }
    void setFillRate   (float f) { fillRate    = jlimit(0.0f,  1.0f,  f); }
    void setComplexity (float c) { complexity  = jlimit(0.0f,  1.0f,  c); }
    void setHold       (float h) { hold        = jlimit(0.0f,  1.0f,  h); }
    void setGhostAmount(float g) { ghostAmount = jlimit(0.0f,  1.0f,  g); }
    void setPatternLen (int   l) { patternLen  = jlimit(1, NUM_STEPS, l); }
    void setSeed       (uint32 s){ seed        = s; rng.seed(seed); }
    void setPhraseBars (int bars) { phraseBars = jlimit (1, 64, bars); }

    int   getStyle()       const { return style; }
    float getTemperature() const { return temperature; }
    float getDensity()     const { return density; }
    float getSwing()       const { return swing; }
    float getHumanize()    const { return humanize; }
    float getVelocity()    const { return velocityMul; }
    float getFillRate()    const { return fillRate; }
    float getComplexity()  const { return complexity; }
    float getHold()        const { return hold; }
    float getGhostAmount() const { return ghostAmount; }
    int   getPatternLen()  const { return patternLen; }
    uint32 getSeed()       const { return seed; }
    int   getPhraseBars()  const { return phraseBars; }

    // Returns sample offset for a given step, applying swing to off-beats
    int getSwingOffset (int step, double samplesPerStep) const;

    // Live playback: recomputes hits from current style/density/etc. so knob changes
    // affect MIDI immediately. When fillHoldActive, adds a live fill layer (hold Fill).
    void evaluateStepForPlayback (int globalStep, int wrappedStep, DrumStep& out,
                                  double samplesPerStep);

    void setFillHoldActive (bool on) { fillHoldActive = on; }
    bool isFillHoldActive() const noexcept { return fillHoldActive; }

    // 16th-note duration in samples — used for humanize amount after generate & for live hits.
    void setPlaybackSamplesPerStep (double s) { playbackSamplesPerStep = juce::jmax (1.0, s); }
    void setHostSampleRate (double sr) { hostSampleRate = juce::jmax (1.0, sr); }

    void setMLPersonalityKnobs (const std::array<float, 10>& k) { mlPersonalityKnobs = k; }
    void captureMLContext();
    void mergePatternFromML (const std::vector<float>& mlOutput);

    // ── Sting/Session features ─────────────────────────────────────────────
    /** 0 = static, 1 = constantly breathing. Drives a slow LFO over humanize / velocity at
        playback time without rewriting the pattern. */
    void  setLifeAmount (float v) noexcept { lifeAmount = juce::jlimit (0.f, 1.f, v); }
    float getLifeAmount() const noexcept   { return lifeAmount; }

    /** Closed/open hat balance for articulation. */
    void  setHatOpen (float v) noexcept { hatOpen = juce::jlimit (0.f, 1.f, v); }
    float getHatOpen() const noexcept   { return hatOpen; }

    /** Velocity contour macro (0 flat, 1 accent, 2 crescendo, 3 decrescendo). */
    void setVelShape (int s) noexcept { velShape = juce::jlimit (0, 3, s); }
    int  getVelShape() const noexcept { return velShape; }

    /** Per-step activity (max velocity per step / 127) — exported for melodic followers. */
    std::array<float, NUM_STEPS> getStepActivityGrid() const noexcept;

    // Called each bar — may auto-trigger a fill based on fillRate
    bool shouldTriggerFill();

    // Register callback so editor can repaint when pattern changes
    std::function<void()> onPatternChanged;

private:
    DrumPattern pattern;
    DrumPattern gridPreview {};
    std::mt19937 rng;

    int   style       = 0;
    float temperature = 1.0f;
    float density     = 0.7f;
    float swing       = 0.0f;
    float humanize    = 0.2f;
    float velocityMul = 0.85f;
    float fillRate    = 0.15f;
    float complexity  = 0.5f;
    float hold        = 0.5f;
    float ghostAmount = 0.5f;
    int   patternLen  = NUM_STEPS;
    uint32 seed       = 42;

    int   barCount    = 0;
    int   phraseBars  = 4; // UI "bar grid": 4/8/16 bars per phrase

    // After generateFill(), play written hits for the fill tail until the last step of that run
    bool fillTailPlayback = false;
    int  fillTailFromStep = 12;

    bool fillHoldActive = false;

    float sampleProb  (float baseProbability) const;
    uint8 sampleVelocity (int step, int drum, bool ghost) const;
    void  applyHumanize  (double samplesPerStep);
    float complexityMod  (int step, int drum) const;

    std::array<float, 10> mlPersonalityKnobs {};
    std::array<float, 32> mlPriorHits {};

    LayerMask layerLocks {};
    float     lifeAmount = 0.0f;
    float     hatOpen    = 0.0f;
    int       velShape   = 1; // 0 flat, 1 accent, 2 crescendo, 3 decrescendo

    static float pseudoRandom01 (uint32_t salt);
    bool         rollHitFromProbability (float prob, uint32_t salt) const;
    uint8        sampleVelocityDeterministic (int step, int drum, bool ghost, uint32_t salt) const;

    float        neutralProbability (int step, int drum) const;
    float        blendedStyleBase (int step, int drum) const;
    float        effectiveSwingAmount() const;
    float        grooveMicroOffset (int step, int drum, uint32_t salt, double samplesPerStep) const;

    void         generateFillDefault (int fromStep);
    void         generateFillFunk (int fromStep);
    void         generateFillDnB (int fromStep);
    void         generateFillJazz (int fromStep);
    void         generateFillLatin (int fromStep);

    double playbackSamplesPerStep = 5512.0; // ~120 BPM 16th @ 44.1 kHz until host calls setPlaybackSamplesPerStep
    double hostSampleRate = 44100.0;
};
