#include "BassEngine.h"
#include "ml/BridgeMLManager.h"
#include <cmath>

using namespace BassPreset;

namespace
{
static float melodicPreviewDet01 (uint32_t seed, int step, uint32_t tag)
{
    uint32_t x = seed ^ (uint32_t) step * 2654435761u ^ tag * 1597334677u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    x *= 3266489917u;
    x ^= x >> 16;
    return (x & 0xffffffu) / (float) 0x1000000;
}

static uint32_t melodicPreviewParamSalt (float density, float swing, float humanize, float hold,
                                         float velocityMul, float fillRate, float complexity,
                                         float ghostAmount, float temperature, float staccato)
{
    auto q = [] (float v) -> uint32_t { return (uint32_t) juce::roundToInt (v * 16384.0f); };
    return q (density) ^ (q (swing) << 1) ^ (q (humanize) << 2) ^ (q (hold) << 3)
           ^ (q (velocityMul) << 4) ^ (q (fillRate) << 5) ^ (q (complexity) << 6)
           ^ (q (ghostAmount) << 7) ^ (q (temperature) << 8) ^ (q (staccato) << 9);
}

/** Two-zone density: skeleton at medium knob, fills scale in top half (see product spec). */
static float probabilityAfterDensity (float base, float density)
{
    float prob;
    if (density <= 0.5f)
        prob = base * (density * 2.0f);
    else
    {
        const float fill = (density - 0.5f) * 2.0f;
        prob = base * (0.5f + 0.5f * fill);
        prob += (1.0f - base) * base * fill * 0.25f;
    }
    return juce::jlimit (0.0f, 1.0f, prob);
}
} // namespace

BassEngine::BassEngine()
    : rng (std::random_device{}())
{
    generatePattern (false, nullptr);
    rebuildGridPreview();
}

void BassEngine::setBassMLKickHint (const std::vector<float>& hint)
{
    bassMLKickHint.assign (16, 0.0f);
    const int n = juce::jmin (16, (int) hint.size());
    for (int i = 0; i < n; ++i)
        bassMLKickHint[(size_t) i] = hint[(size_t) i];
}

void BassEngine::setBassMLPersonalityKnobs (const std::vector<float>& knobs)
{
    bassMLPersonalityKnobs.assign (10, 0.5f);
    const int n = juce::jmin (10, (int) knobs.size());
    for (int i = 0; i < n; ++i)
        bassMLPersonalityKnobs[(size_t) i] = juce::jlimit (0.0f, 1.0f, knobs[(size_t) i]);
}

void BassEngine::captureBassMLContextFromPattern()
{
    bassMLContext.resize (64);
    const float sr = (float) juce::jmax (1.0, hostSampleRate);
    const float shiftRef = sr * 0.05f;

    for (int s = 0; s < 16; ++s)
    {
        const auto& h = pattern[(size_t) s];
        const size_t b = (size_t) s * 4;
        bassMLContext[b + 0] = h.active ? (float) (h.midiNote % 12) / 11.0f : 0.0f;
        bassMLContext[b + 1] = h.active ? 1.0f : 0.0f;
        bassMLContext[b + 2] = h.active ? (float) h.velocity / 127.0f : 0.0f;
        bassMLContext[b + 3] = juce::jlimit (-1.0f, 1.0f, (float) h.timeShift / shiftRef);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Root MIDI note: C in the chosen octave
// octave=2 → C2 = MIDI 36 (classic electric bass register)
// ─────────────────────────────────────────────────────────────────────────────
int BassEngine::rootMidiBase() const
{
    return rootNote + (octave + 1) * 12;
}

// ─────────────────────────────────────────────────────────────────────────────
// degreeToMidiNote
//
// Converts a degree index (0–7) to an absolute MIDI note.
// Degree 6 (approach) is handled here: -1 semitone below prevMidi.
// Degree 7 (sub) goes an octave below root.
// All other degrees use DEGREE_SEMITONES[] relative to rootMidiBase().
// ─────────────────────────────────────────────────────────────────────────────
int BassEngine::degreeToMidiNote (int degree, int prevMidi) const
{
    degree = jlimit (0, NUM_DEGREES - 1, degree);

    if (degree == 6) // Chromatic approach
    {
        // If we know where we're going, land one semitone below; else use b7
        return (prevMidi > 0) ? (prevMidi - 1) : (rootMidiBase() + 10);
    }

    return rootMidiBase() + DEGREE_SEMITONES[degree];
}

int BassEngine::nearestDegreeForMidi (int midi, int prevMidi) const
{
    int best = 0;
    int bestDist = 128;
    for (int d = 0; d < NUM_DEGREES; ++d)
    {
        int m = degreeToMidiNote (d, prevMidi);
        int dist = std::abs (m - midi);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = d;
        }
    }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
// generatePattern
// ─────────────────────────────────────────────────────────────────────────────
void BassEngine::generatePatternRange (int fromStep0, int toStep0, bool seamlessPerform, BridgeMLManager* ml)
{
    if (locked) return;

    captureBassMLContextFromPattern();

    fromStep0 = juce::jlimit (0, NUM_STEPS - 1, fromStep0);
    toStep0   = juce::jlimit (0, NUM_STEPS - 1, toStep0);
    if (toStep0 < fromStep0)
        std::swap (fromStep0, toStep0);

    std::array<BassHit, NUM_STEPS> previous = pattern;

    for (int step = fromStep0; step <= toStep0; ++step)
    {
        if (step >= patternLen)
            break;

        bool mutateThisStep = true;
        if (seamlessPerform)
        {
            float mutateProb = 0.15f + 0.20f * (complexity + (1.0f - density));
            if (previous[(size_t) step].active == false) mutateProb *= 1.2f;
            mutateProb = std::pow (mutateProb, 1.0f / temperature);
            mutateThisStep = sampleProb (mutateProb);
        }

        if (! mutateThisStep)
        {
            pattern[(size_t) step] = previous[(size_t) step];
            continue;
        }

        float base = BASS_NOTE_PROBS[style][step];
        float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (step));
        prob = probabilityAfterDensity (prob, density);

        bool active = sampleProb (prob);
        const BassHit& prev = previous[(size_t) step];

        if (! active)
        {
            pattern[(size_t) step] = BassHit{};
            continue;
        }

        int   prefDeg = BASS_PREFERRED_DEGREE[style][step];
        int   deg     = chooseDegreeProbabilistic (step, prefDeg);

        float ghostTend  = BASS_GHOST_TENDENCY[style][step] * ghostAmount;
        bool  isGhost    = (deg != 6) && sampleProb (ghostTend);
        bool  isAccent   = (step % 4 == 0) && ! isGhost;
        int midi = degreeToMidiNote (deg, -1);

        pattern[(size_t) step].active    = true;
        pattern[(size_t) step].degree    = deg;
        pattern[(size_t) step].midiNote  = midi;

        if (prev.active)
        {
            pattern[(size_t) step].velocity  = prev.velocity;
            pattern[(size_t) step].isGhost   = prev.isGhost;
            pattern[(size_t) step].timeShift = prev.timeShift;
        }
        else
        {
            pattern[(size_t) step].velocity  = sampleVelocity (step, isGhost, isAccent);
            pattern[(size_t) step].isGhost   = isGhost;
            pattern[(size_t) step].timeShift   = 0;
        }
    }

    for (int step = patternLen; step < NUM_STEPS; ++step)
        pattern[(size_t) step] = BassHit{};

    resolveApproachNotes();

    const bool partialRegenWindow = (fromStep0 > 0 || toStep0 < patternLen - 1);
    if (! partialRegenWindow)
        mergePatternFromML (ml);

    if (onPatternChanged)
        onPatternChanged();

    rebuildGridPreview();
}

void BassEngine::generatePattern (bool seamlessPerform, BridgeMLManager* ml)
{
    generatePatternRange (0, patternLen - 1, seamlessPerform, ml);
}

void BassEngine::mergePatternFromML (BridgeMLManager* ml)
{
    if (ml == nullptr || ! ml->hasModel (BridgeMLManager::ModelType::BassModel))
        return;
    if (bassMLKickHint.size() < 16)
        return;

    const auto out = ml->generateBass (bassMLPersonalityKnobs, bassMLKickHint, rootNote, scale, style, bassMLContext);
    if (out.size() < 64)
        return;

    for (int s = 0; s < patternLen && s < 16; ++s)
    {
        const float act = out[48 + (size_t) s];
        if (act < 0.5f)
        {
            pattern[(size_t) s] = BassHit{};
            continue;
        }

        const int pc = juce::jlimit (0, 11, (int) std::lround (out[(size_t) s]));
        const int midi = juce::jlimit (12, 127, rootMidiBase() + pc);
        const float offMs = out[16 + (size_t) s] * 30.0f;
        const int vel = juce::jlimit (1, 127, (int) std::lround (out[32 + (size_t) s] * 127.0f));

        BassHit& h = pattern[(size_t) s];
        h.active = true;
        h.midiNote = midi;
        h.velocity = (uint8) vel;
        h.isGhost = false;
        h.timeShift = (int) (offMs * static_cast<float> (hostSampleRate) / 1000.0f);
        const int prevMidi = s > 0 ? pattern[(size_t) (s - 1)].midiNote : -1;
        h.degree = nearestDegreeForMidi (midi, prevMidi);
    }

    resolveApproachNotes();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildGridPreview — pattern shown in UI; live density preview + fill-hold (Drums-style)
// ─────────────────────────────────────────────────────────────────────────────
void BassEngine::rebuildGridPreview()
{
    displayPattern = pattern;
    for (int s = 0; s < patternLen; ++s)
    {
        if (! pattern[(size_t) s].active)
            continue;
        BassHit& out = displayPattern[(size_t) s];
        const int vel = juce::roundToInt ((float) pattern[(size_t) s].velocity * velocityMul);
        out.velocity = (uint8) juce::jlimit (1, 127, vel);
    }

    if (fillHoldActive)
    {
        for (int s = 0; s < patternLen; ++s)
        {
            if (! pattern[(size_t) s].active)
                continue;
            auto& out = displayPattern[(size_t) s];
            if (out.velocity < 110)
                out.velocity = (uint8) jmin (127, (int) out.velocity + 20);
        }
    }
}

void BassEngine::morphPatternForDensityAndComplexity (int rangeFromStep0, int rangeToStep0)
{
    if (locked)
        return;

    int r0 = rangeFromStep0;
    int r1 = rangeToStep0;
    if (r0 < 0 || r1 < 0)
    {
        r0 = 0;
        r1 = patternLen - 1;
    }
    r0 = juce::jlimit (0, patternLen - 1, r0);
    r1 = juce::jlimit (0, patternLen - 1, r1);
    if (r1 < r0)
        std::swap (r0, r1);

    const int rangeLen = r1 - r0 + 1;
    const int minTarget  = (density <= 0.001f ? 0 : juce::jmax (0, (int) std::lround ((float) rangeLen * 0.07f)));
    const int maxTarget  = rangeLen;
    int targetInRange = (density >= 0.999f ? maxTarget
                                          : (int) std::lround (juce::jmap (density, 0.f, 1.f, (float) minTarget, (float) maxTarget)));
    targetInRange = juce::jlimit (0, rangeLen, targetInRange);

    auto countActiveInRange = [&] {
        int c = 0;
        for (int s = r0; s <= r1; ++s)
            if (pattern[(size_t) s].active)
                ++c;
        return c;
    };

    int activeInRange = countActiveInRange();

    while (activeInRange > targetInRange)
    {
        int bestS = -1;
        float bestScore = 1e9f;
        for (int s = r0; s <= r1; ++s)
        {
            if (! pattern[(size_t) s].active)
                continue;
            const float downProt = (s % 4 == 0) ? 0.5f : 0.f;
            const float score = BASS_NOTE_PROBS[style][s] + downProt;
            if (score < bestScore)
            {
                bestScore = score;
                bestS = s;
            }
        }
        if (bestS < 0)
            break;
        pattern[(size_t) bestS] = BassHit{};
        --activeInRange;
    }

    while (activeInRange < targetInRange)
    {
        int bestS = -1;
        float bestScore = -1.f;
        for (int s = r0; s <= r1; ++s)
        {
            if (pattern[(size_t) s].active)
                continue;
            const float score = BASS_NOTE_PROBS[style][s] * (1.0f + 0.22f * complexityMod (s));
            if (score > bestScore)
            {
                bestScore = score;
                bestS = s;
            }
        }
        if (bestS < 0)
            break;
        const int step = bestS;
        const int prefDeg = BASS_PREFERRED_DEGREE[style][step];
        int prevMidi = -1;
        for (int ps = step - 1; ps >= 0; --ps)
        {
            if (pattern[(size_t) ps].active)
            {
                prevMidi = pattern[(size_t) ps].midiNote;
                break;
            }
        }
        const int deg = chooseDegreeForMorphAdd (step, prefDeg, prevMidi);
        const float ghostT = jlimit (0.0f, 1.0f, BASS_GHOST_TENDENCY[style][step] * ghostAmount);
        const bool ghost = (deg != 6) && sampleProb (ghostT);
        const bool accent = (step % 4 == 0) && ! ghost;
        pattern[(size_t) step].active = true;
        pattern[(size_t) step].degree = deg;
        pattern[(size_t) step].midiNote = degreeToMidiNote (deg, prevMidi);
        pattern[(size_t) step].velocity = sampleVelocity (step, ghost, accent);
        pattern[(size_t) step].isGhost = ghost;
        pattern[(size_t) step].timeShift = 0;
        ++activeInRange;
    }

    for (int s = r0; s <= r1; ++s)
    {
        if (! pattern[(size_t) s].active)
            continue;
        int prevMidi = -1;
        for (int ps = s - 1; ps >= 0; --ps)
            if (pattern[(size_t) ps].active)
            {
                prevMidi = pattern[(size_t) ps].midiNote;
                break;
            }
        const float roll = (const_cast<BassEngine*> (this)->rng() / float (std::mt19937::max()));
        if (roll < complexity * 0.14f)
        {
            const int prefDeg = BASS_PREFERRED_DEGREE[style][s];
            const int deg = chooseDegreeProbabilistic (s, prefDeg);
            pattern[(size_t) s].degree = deg;
            pattern[(size_t) s].midiNote = degreeToMidiNote (deg, prevMidi);
        }
        else if (complexity < 0.32f && roll < 0.18f)
        {
            const int prefDeg = BASS_PREFERRED_DEGREE[style][s];
            pattern[(size_t) s].degree = prefDeg;
            pattern[(size_t) s].midiNote = degreeToMidiNote (prefDeg, prevMidi);
        }
    }

    resolveApproachNotes();
    rebuildGridPreview();
    if (onPatternChanged != nullptr)
        onPatternChanged();
}

void BassEngine::adaptPatternToNewStyle (int newStyleIndex)
{
    newStyleIndex = juce::jlimit (0, BassPreset::NUM_STYLES - 1, newStyleIndex);
    style = newStyleIndex;

    for (int s = 0; s < patternLen; ++s)
    {
        if (! pattern[(size_t) s].active)
            continue;
        const float t = melodicPreviewDet01 (seed, s, 0xADA0u ^ (uint32_t) s * 1315423911u);
        if (t < 0.38f)
            continue;

        const int prefDeg = BASS_PREFERRED_DEGREE[style][s];
        const int deg = chooseDegreeProbabilistic (s, prefDeg);
        int prevMidi = -1;
        for (int ps = s - 1; ps >= 0; --ps)
        {
            if (pattern[(size_t) ps].active)
            {
                prevMidi = pattern[(size_t) ps].midiNote;
                break;
            }
        }
        pattern[(size_t) s].degree = deg;
        pattern[(size_t) s].midiNote = degreeToMidiNote (deg, prevMidi);
        const float ghostT = jlimit (0.0f, 1.0f, BASS_GHOST_TENDENCY[style][s] * ghostAmount);
        pattern[(size_t) s].isGhost = (deg != 6) && sampleProb (ghostT);
    }

    resolveApproachNotes();
    rebuildGridPreview();
    if (onPatternChanged != nullptr)
        onPatternChanged();
}

void BassEngine::evolvePatternRangeForJam (int fromStep0, int toStep0, BridgeMLManager* ml)
{
    juce::ignoreUnused (ml);
    if (locked)
        return;

    fromStep0 = juce::jlimit (0, patternLen - 1, fromStep0);
    toStep0 = juce::jlimit (0, patternLen - 1, toStep0);
    if (toStep0 < fromStep0)
        std::swap (fromStep0, toStep0);

    std::array<BassHit, NUM_STEPS> previous = pattern;

    for (int step = fromStep0; step <= toStep0; ++step)
    {
        const float t = melodicPreviewDet01 (seed, step, 0xE901u ^ (uint32_t) step * 17389u);
        if (t < 0.66f && previous[(size_t) step].active)
        {
            pattern[(size_t) step] = previous[(size_t) step];
            continue;
        }

        float base = BASS_NOTE_PROBS[style][step];
        float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (step));
        prob = probabilityAfterDensity (prob, density);
        const bool active = sampleProb (prob);

        if (! active)
        {
            pattern[(size_t) step] = BassHit{};
            continue;
        }

        const int prefDeg = BASS_PREFERRED_DEGREE[style][step];
        const int deg = chooseDegreeProbabilistic (step, prefDeg);
        const float ghostT = jlimit (0.0f, 1.0f, BASS_GHOST_TENDENCY[style][step] * ghostAmount);
        const bool isGhost = (deg != 6) && sampleProb (ghostT);
        const bool isAccent = (step % 4 == 0) && ! isGhost;
        int prevMidi = -1;
        for (int ps = step - 1; ps >= 0; --ps)
        {
            if (pattern[(size_t) ps].active)
            {
                prevMidi = pattern[(size_t) ps].midiNote;
                break;
            }
        }
        if (prevMidi < 0)
            for (int ps = step - 1; ps >= 0; --ps)
                if (previous[(size_t) ps].active)
                {
                    prevMidi = previous[(size_t) ps].midiNote;
                    break;
                }

        pattern[(size_t) step].active = true;
        pattern[(size_t) step].degree = deg;
        pattern[(size_t) step].midiNote = degreeToMidiNote (deg, prevMidi);

        if (previous[(size_t) step].active)
        {
            pattern[(size_t) step].velocity  = previous[(size_t) step].velocity;
            pattern[(size_t) step].isGhost   = previous[(size_t) step].isGhost;
            pattern[(size_t) step].timeShift = previous[(size_t) step].timeShift;
        }
        else
        {
            pattern[(size_t) step].velocity  = sampleVelocity (step, isGhost, isAccent);
            pattern[(size_t) step].isGhost   = isGhost;
            pattern[(size_t) step].timeShift   = 0;
        }
    }

    resolveApproachNotes();
    rebuildGridPreview();
    if (onPatternChanged != nullptr)
        onPatternChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// generateFill
//
// Classic bass fill: chromatic/octave run into the downbeat.
// fromStep–15: ascending scale run with increasing density and velocity.
// ─────────────────────────────────────────────────────────────────────────────
void BassEngine::generateFill (int fromStep)
{
    int fillSteps = NUM_STEPS - fromStep;

    // Scale tones for an ascending run (use current scale)
    const int* intervals = SCALE_INTERVALS[scale];
    const int  toneCount = SCALE_TONE_COUNT[scale];
    int        base      = rootMidiBase();

    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        pattern[(size_t)i] = BassHit{};

        int  fillIdx  = i - fromStep;
        float progress = (float)fillIdx / (float)fillSteps;

        // Fill note: walk up the scale
        int scaleIdx = (int)(progress * (float)(toneCount - 1));
        scaleIdx = jlimit (0, toneCount - 1, scaleIdx);
        int semi = intervals[scaleIdx];
        if (semi < 0) semi = 0;

        // Last two steps: set up for downbeat with root or chromatic approach
        if (i == NUM_STEPS - 2)
        {
            // One chromatic below root
            pattern[(size_t)i].active    = true;
            pattern[(size_t)i].midiNote  = base - 1;
            pattern[(size_t)i].degree    = 6;  // approach
            pattern[(size_t)i].velocity  = (uint8)(95 + (int)(25.0f * progress));
            pattern[(size_t)i].isGhost   = false;
            continue;
        }

        if (i == NUM_STEPS - 1)
        {
            // Octave up for big landing
            pattern[(size_t)i].active    = true;
            pattern[(size_t)i].midiNote  = base + 12;
            pattern[(size_t)i].degree    = 5;  // Oct
            pattern[(size_t)i].velocity  = 127;
            pattern[(size_t)i].isGhost   = false;
            continue;
        }

        // Escalating hits — every step fires from halfway through, denser at end
        bool fires = (progress > 0.35f) || ((fillIdx % jmax (1, (int)(3.0f * (1.0f - progress)))) == 0);
        if (fires)
        {
            pattern[(size_t)i].active    = true;
            pattern[(size_t)i].midiNote  = base + semi;
            pattern[(size_t)i].degree    = (scaleIdx < NUM_DEGREES) ? scaleIdx : 0;
            pattern[(size_t)i].velocity  = (uint8)(78 + (int)(42.0f * progress));
            pattern[(size_t)i].isGhost   = false;
        }
    }

    if (onPatternChanged)
        onPatternChanged();

    rebuildGridPreview();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

float BassEngine::sampleProb (float p) const
{
    float adjusted = std::pow (jlimit (0.0f, 1.0f, p), 1.0f / temperature);
    return (const_cast<BassEngine*>(this)->rng() / float (std::mt19937::max())) < adjusted;
}

uint8 BassEngine::sampleVelocity (int step, bool ghost, bool accent) const
{
    int vMin, vMax;
    if (ghost)
    {
        vMin = GHOST_VELOCITY_RANGE[style][0];
        vMax = GHOST_VELOCITY_RANGE[style][1];
    }
    else
    {
        vMin = STYLE_VELOCITY_RANGE[style][0];
        vMax = STYLE_VELOCITY_RANGE[style][1];

        if (accent)
        {
            float boost = STYLE_ACCENT_STRENGTH[style];
            vMin = (int)(vMin + (vMax - vMin) * boost * 0.5f);
        }
    }

    int vel = const_cast<BassEngine*>(this)->rng() % jmax (1, vMax - vMin + 1) + vMin;
    vel     = (int)(vel * velocityMul);
    return (uint8)jlimit (1, 127, vel);
}

int BassEngine::chooseDegreeForMorphAdd (int step, int preferredDegree, int prevMidi) const
{
    if (prevMidi < 0)
        return chooseDegreeProbabilistic (step, preferredDegree);

    const float stayProb = juce::jmap (complexity, 0.f, 1.f, 0.90f, 0.20f);
    const float roll = (const_cast<BassEngine*> (this)->rng() / float (std::mt19937::max()));
    if (roll < stayProb)
        return preferredDegree;

    return chooseDegreeProbabilistic (step, preferredDegree);
}

int BassEngine::chooseDegreeProbabilistic (int step, int preferredDegree) const
{
    // At low complexity, almost always use the preferred degree.
    // At high complexity, occasionally substitute adjacent chord tones.
    if (complexity < 0.45f)
        return preferredDegree;

    float deviateProbability = (complexity - 0.45f) * 0.55f; // up to ~0.30

    // Only deviate on off-beat steps (1,2,3, 5,6,7, …)
    bool offBeat = (step % 4 != 0);
    if (!offBeat) return preferredDegree;

    if (sampleProb (deviateProbability))
    {
        // Choose a random valid alternate degree (avoid approach/sub on inner steps)
        const int alts[] = { 0, 3, 4, 5, 2, 1 };
        int idx = const_cast<BassEngine*>(this)->rng() % 6;
        return alts[idx];
    }

    return preferredDegree;
}

float BassEngine::complexityMod (int step) const
{
    const bool offBeat = (step % 2 != 0);
    const float mod    = (complexity - 0.5f) * 0.5f;
    const float weight = offBeat ? 1.4f : 0.4f;
    return mod * weight;
}

void BassEngine::resolveApproachNotes()
{
    // Approach (degree 6): look at the next active note's MIDI pitch
    // and set this note to that pitch - 1 (chromatic approach from below).
    for (int step = 0; step < patternLen; ++step)
    {
        if (!pattern[(size_t)step].active)  continue;
        if (pattern[(size_t)step].degree != 6) continue;

        // Find next active note
        int  nextMidi = rootMidiBase();  // fallback: approach to root
        for (int s = step + 1; s < patternLen; ++s)
        {
            if (pattern[(size_t)s].active)
            {
                nextMidi = pattern[(size_t)s].midiNote;
                break;
            }
        }

        pattern[(size_t)step].midiNote = jlimit (12, 127, nextMidi - 1);
    }
}

int BassEngine::resolveApproachNote (int step) const
{
    int nextMidi = rootMidiBase();
    for (int s = step + 1; s < patternLen; ++s)
    {
        if (pattern[(size_t)s].active)
        {
            nextMidi = pattern[(size_t)s].midiNote;
            break;
        }
    }
    return jlimit (12, 127, nextMidi - 1);
}

void BassEngine::applyHumanize (double samplesPerStep)
{
    if (humanize < 0.01f) return;

    int maxShift = (int)(samplesPerStep * 0.18 * humanize);
    if (maxShift < 1) return;

    for (int step = 0; step < patternLen; ++step)
    {
        if (!pattern[(size_t)step].active) continue;

        const float u = melodicPreviewDet01 (seed, step, 0x48ABu ^ (uint32_t) step * 1103515245u);
        int jitter = (int) std::lround ((u * 2.0 - 1.0) * (double) maxShift);

        // Residual timing-feel offset: high hold → less micro-shift (hold is primarily note length).
        float feel = BASS_TIMING_FEEL[style][step];
        const float tightness = (1.0f - hold * 0.92f) * 0.5f;
        int feelOff = (int)(feel * tightness * samplesPerStep);

        pattern[(size_t)step].timeShift = jitter + feelOff;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Swing offset: delays off-beat 8th notes (steps 2, 6, 10, 14)
// ─────────────────────────────────────────────────────────────────────────────
int BassEngine::getSwingOffset (int step, double samplesPerStep) const
{
    if (swing < 0.01f) return 0;

    // Off-beat 8th notes at steps 2, 6, 10, 14
    bool isOffBeat8th = ((step % 4) == 2);
    if (!isOffBeat8th) return 0;

    // Push toward triplet feel: 50% (straight) → 66% (full swing)
    double swingFraction = 0.50 + swing * 0.16;
    double offset        = (swingFraction - 0.50) * samplesPerStep * 2.0;
    return (int)offset;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timing feel offset: separate from swing, applies per-step styling
// This is pre-baked into timeShift during generation via applyHumanize.
// This method is here for external callers who want the raw feel value.
// ─────────────────────────────────────────────────────────────────────────────
int BassEngine::getTimingFeelOffset (int step, double samplesPerStep) const
{
    float feel = BASS_TIMING_FEEL[style][step];
    const float tightness = (1.0f - hold * 0.92f) * 0.5f;
    return (int)(feel * tightness * samplesPerStep);
}

// ─────────────────────────────────────────────────────────────────────────────
// calcNoteDuration
//
// Returns note-on duration in samples.
// Staccato=0 → legato (90% of step), Staccato=1 → very short (20% of step).
// Ghost notes always get short duration.
// ─────────────────────────────────────────────────────────────────────────────
int BassEngine::calcNoteDuration (const BassHit& hit, double samplesPerStep) const
{
    float artDur = hit.isGhost ? ARTICULATION_DURATION[2]
                               : ARTICULATION_DURATION[0];

    // Staccato scales down normal notes but not ghosts
    if (!hit.isGhost)
    {
        float stacScale = 1.0f - staccato * 0.70f;  // 1.0 → 0.30
        artDur *= stacScale;
    }

    artDur *= juce::jmap (hold, 0.0f, 1.0f, 0.45f, 1.55f);

    return jmax (1, (int)(artDur * samplesPerStep));
}

// ─────────────────────────────────────────────────────────────────────────────
// shouldTriggerFill
// ─────────────────────────────────────────────────────────────────────────────
bool BassEngine::shouldTriggerFill()
{
    ++barCount;
    if (fillRate < 0.01f) return false;

    float roll = const_cast<BassEngine*>(this)->rng() / float (std::mt19937::max());
    const int gridBars = jmax (1, phraseBars);
    float prob = fillRate * (1.0f + (barCount % gridBars == 0 ? 0.5f : 0.0f));
    return roll < prob * 0.25f;
}

int BassEngine::snapMidiToCurrentScale (int midi) const
{
    const int* iv = SCALE_INTERVALS[scale];
    const int n = SCALE_TONE_COUNT[scale];
    const int base = rootMidiBase();
    int best = juce::jlimit (12, 127, midi);
    int bestDist = 1000;

    for (int oct = -3; oct <= 3; ++oct)
    {
        for (int t = 0; t < n; ++t)
        {
            if (iv[t] < 0)
                continue;
            const int note = base + oct * 12 + iv[t];
            if (note < 1 || note > 127)
                continue;
            const int d = std::abs (note - midi);
            if (d < bestDist)
            {
                bestDist = d;
                best = note;
            }
        }
    }
    return juce::jlimit (12, 127, best);
}

void BassEngine::remapPatternAfterTonalityChange (int oldRootMidiBase, int oldScaleIdx)
{
    const int newBase = rootMidiBase();
    const int delta = newBase - oldRootMidiBase;
    const bool scaleChanged = (oldScaleIdx != scale);

    for (int step = 0; step < patternLen; ++step)
    {
        auto& h = pattern[(size_t) step];
        if (! h.active)
            continue;
        h.midiNote = juce::jlimit (0, 127, h.midiNote + delta);
    }

    if (scaleChanged)
    {
        for (int step = 0; step < patternLen; ++step)
        {
            auto& h = pattern[(size_t) step];
            if (! h.active)
                continue;
            h.midiNote = snapMidiToCurrentScale (h.midiNote);
        }
    }

    int prev = -1;
    for (int step = 0; step < patternLen; ++step)
    {
        auto& h = pattern[(size_t) step];
        if (! h.active)
            continue;
        h.degree = nearestDegreeForMidi (h.midiNote, prev);
        h.midiNote = degreeToMidiNote (h.degree, prev);
        prev = h.midiNote;
    }

    resolveApproachNotes();
    rebuildGridPreview();

    if (onPatternChanged)
        onPatternChanged();
}
