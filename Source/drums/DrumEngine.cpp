#include "DrumEngine.h"
#include "ml/BridgeMLManager.h"
#include <cmath>
#include <tuple>
#include <vector>

namespace
{
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

DrumEngine::DrumEngine()
    : rng (std::random_device{}())
{
    mlPersonalityKnobs.fill (0.5f);
    generatePattern (false, nullptr);
}

// ─── Core generation ──────────────────────────────────────────────────────────

void DrumEngine::generatePatternRange (int from0, int to0, bool seamlessPerform, BridgeMLManager* ml)
{
    from0 = juce::jlimit (0, NUM_STEPS - 1, from0);
    to0   = juce::jlimit (0, NUM_STEPS - 1, to0);
    if (to0 < from0)
        std::swap (from0, to0);

    captureMLContext();

    const DrumPattern previous = pattern;
    std::vector<std::tuple<int, int, int>> restoreTimeShift;

    for (int step = from0; step <= to0; ++step)
    {
        if (step >= patternLen)
            break;

        for (int drum = 0; drum < NUM_DRUMS; ++drum)
        {
            bool mutateThisStep = true;
            if (seamlessPerform)
            {
                float mutateProb = 0.15f + 0.20f * (complexity + (1.0f - density));
                if (drum == 2 || drum == 3) mutateProb *= 1.3f;
                mutateProb = std::pow (mutateProb, 1.0f / temperature);
                mutateThisStep = pseudoRandom01 (rng()) < mutateProb;
            }

            if (! mutateThisStep)
            {
                pattern[step][drum] = previous[step][drum];
                if (pattern[step][drum].active)
                    restoreTimeShift.emplace_back (step, drum, previous[step][drum].timeShift);
                continue;
            }

            float base = blendedStyleBase (step, drum);
            float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (step, drum));
            prob = probabilityAfterDensity (prob, density);

            bool active = sampleProb (prob);

            if (! active)
            {
                pattern[step][drum] = {};
                continue;
            }

            const auto& prev = previous[step][drum];
            pattern[step][drum].active = true;

            if (prev.active)
            {
                pattern[step][drum].velocity  = prev.velocity;
                pattern[step][drum].timeShift = prev.timeShift;
                restoreTimeShift.emplace_back (step, drum, prev.timeShift);
            }
            else
            {
                const float ghostThresh = juce::jmap (ghostAmount, 0.42f, 0.10f);
                const bool ghost = (base < ghostThresh);
                pattern[step][drum].velocity  = sampleVelocity (step, drum, ghost);
                pattern[step][drum].timeShift = 0;
            }
        }
    }

    for (int step = patternLen; step < NUM_STEPS; ++step)
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[step][drum].active = false;

    const bool partialRegenWindow = (from0 > 0 || to0 < patternLen - 1);
    std::vector<float> mlOut;
    if (! partialRegenWindow && ml != nullptr && ml->hasModel (BridgeMLManager::ModelType::DrumHumanizer))
    {
        const std::array<float, 4> groove {
            juce::jlimit (0.0f, 1.0f, swing),
            juce::jlimit (0.0f, 1.0f, density),
            juce::jlimit (0.0f, 1.0f, velocityMul * 0.85f + fillRate * 0.15f),
            juce::jlimit (0.0f, 1.0f, complexity * 0.65f + humanize * 0.35f),
        };
        mlOut = ml->generateDrums (mlPersonalityKnobs, mlPriorHits, groove);
    }
    if (! partialRegenWindow)
        mergePatternFromML (mlOut);
    applyHumanize (playbackSamplesPerStep);

    for (const auto& t : restoreTimeShift)
        pattern[(size_t) std::get<0> (t)][(size_t) std::get<1> (t)].timeShift = std::get<2> (t);

    if (onPatternChanged)
        onPatternChanged();
}

void DrumEngine::generatePattern (bool seamlessPerform, BridgeMLManager* ml)
{
    generatePatternRange (0, patternLen - 1, seamlessPerform, ml);
}

void DrumEngine::generateFill (int fromStep)
{
    fillTailFromStep = juce::jlimit (0, NUM_STEPS - 1, fromStep);
    fillTailPlayback = (fillTailFromStep < patternLen);

    switch (STYLE_FILL_FAMILY[juce::jlimit (0, NUM_STYLES - 1, style)])
    {
        case 1:  generateFillFunk (fromStep);  break;
        case 2:  generateFillDnB (fromStep);   break;
        case 3:  generateFillJazz (fromStep);  break;
        case 4:  generateFillLatin (fromStep); break;
        default: generateFillDefault (fromStep); break;
    }

    applyHumanize (playbackSamplesPerStep);

    if (onPatternChanged)
        onPatternChanged();
}

void DrumEngine::generateFillDefault (int fromStep)
{
    int fillSteps = juce::jmax (1, NUM_STEPS - fromStep);

    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        int fillIdx = i - fromStep;

        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[i][drum].active = false;

        float progress = (float)fillIdx / (float)fillSteps;

        if (fillIdx % juce::jmax (1, (int)(4.0f * (1.0f - progress))) == 0)
        {
            pattern[i][1].active   = true;
            pattern[i][1].velocity = (uint8)(90 + (int)(30.0f * progress));
        }

        int tomDrum = 6 - (int)(progress * 3.0f);
        tomDrum = juce::jlimit (4, 6, tomDrum);
        if ((fillIdx % 2) == 0 || progress > 0.6f)
        {
            pattern[i][tomDrum].active   = true;
            pattern[i][tomDrum].velocity = (uint8)(80 + (int)(35.0f * progress));
        }

        if (i == NUM_STEPS - 1)
        {
            pattern[i][0].active   = true;
            pattern[i][0].velocity = 127;
        }
    }
}

void DrumEngine::generateFillFunk (int fromStep)
{
    generateFillDefault (fromStep);
    // Pocket ghosts + syncopated kicks into the one
    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        int fi = i - fromStep;
        float p = (float)fi / (float)juce::jmax (1, NUM_STEPS - fromStep);
        if ((i % 2) == 1 && pattern[i][1].active == false)
        {
            pattern[i][1].active = (pseudoRandom01 ((uint32_t)i * 1315423911u) < 0.45f + p * 0.35f);
            if (pattern[i][1].active)
                pattern[i][1].velocity = (uint8) juce::jlimit (28, 78, 35 + (int)(40.0f * p));
        }
        if (i >= NUM_STEPS - 2)
        {
            pattern[i][0].active = true;
            pattern[i][0].velocity = (uint8) juce::jlimit (70, 118, 85 + (int)(25.0f * p));
        }
    }
}

void DrumEngine::generateFillDnB (int fromStep)
{
    int fillSteps = juce::jmax (1, NUM_STEPS - fromStep);
    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[i][drum].active = false;

        int fi = i - fromStep;
        float pr = (float)fi / (float)fillSteps;
        // Rolling snares + double kicks
        if ((fi % 2) == 0)
        {
            pattern[i][1].active = true;
            pattern[i][1].velocity = (uint8)(70 + (int)(45.0f * pr));
        }
        if ((fi % 3) == 0)
        {
            pattern[i][0].active = true;
            pattern[i][0].velocity = (uint8)(95 + (int)(20.0f * pr));
        }
        pattern[i][2].active = (fi % 2) == 0;
        if (pattern[i][2].active)
            pattern[i][2].velocity = (uint8)(55 + (int)(25.0f * pr));
    }
    pattern[NUM_STEPS - 1][0].active = true;
    pattern[NUM_STEPS - 1][0].velocity = 120;
}

void DrumEngine::generateFillJazz (int fromStep)
{
    int fillSteps = juce::jmax (1, NUM_STEPS - fromStep);
    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[i][drum].active = false;
        int fi = i - fromStep;
        float pr = (float)fi / (float)fillSteps;
        int tom = 4 + (fi % 3);
        pattern[i][tom].active = true;
        pattern[i][tom].velocity = (uint8) juce::jlimit (40, 95, 55 + (int)(30.0f * pr));
        if ((fi % 4) == 0)
        {
            pattern[i][8].active = true;
            pattern[i][8].velocity = (uint8) juce::jlimit (45, 85, 60 + (int)(15.0f * pr));
        }
    }
}

void DrumEngine::generateFillLatin (int fromStep)
{
    int fillSteps = juce::jmax (1, NUM_STEPS - fromStep);
    for (int i = fromStep; i < NUM_STEPS; ++i)
    {
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[i][drum].active = false;
        int fi = i - fromStep;
        float pr = (float)fi / (float)fillSteps;
        int conga = (fi % 2 == 0) ? 4 : 5;
        pattern[i][conga].active = true;
        pattern[i][conga].velocity = (uint8) juce::jlimit (50, 110, 65 + (int)(35.0f * pr));
        if ((fi % 3) == 0)
        {
            pattern[i][0].active = true;
            pattern[i][0].velocity = (uint8) juce::jlimit (72, 115, 88 + (int)(12.0f * pr));
        }
    }
    pattern[NUM_STEPS - 1][6].active = true;
    pattern[NUM_STEPS - 1][6].velocity = 85;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

float DrumEngine::pseudoRandom01 (uint32_t salt)
{
    salt ^= salt << 13;
    salt ^= salt >> 17;
    salt ^= salt << 5;
    return (salt & 0xffffffu) / 16777216.0f;
}

float DrumEngine::neutralProbability (int step, int drum) const
{
    const bool down = (step % 4 == 0);
    const bool back = (step % 8 == 4);
    switch (drum)
    {
        case 0: return down ? 0.48f : 0.11f;
        case 1: return back ? 0.52f : 0.09f;
        case 2: return 0.34f;
        case 3: return 0.06f;
        default: return 0.07f;
    }
}

float DrumEngine::blendedStyleBase (int step, int drum) const
{
    return stylePatternAt (style, drum, step);
}

float DrumEngine::effectiveSwingAmount() const
{
    const float styleBias = STYLE_DEFAULT_SWING[juce::jlimit (0, NUM_STYLES - 1, style)] * 0.45f;
    return juce::jlimit (0.0f, 1.0f, swing + styleBias);
}

float DrumEngine::grooveMicroOffset (int step, int drum, uint32_t salt, double samplesPerStep) const
{
    const float loose = STYLE_GROOVE_LOOSENESS[juce::jlimit (0, NUM_STYLES - 1, style)];
    const float amt = loose * (0.35f + 0.65f * humanize)
                        * juce::jmap (hold, 0.35f, 1.0f);
    if (amt < 0.03f) return 0.0f;
    const int maxNudge = juce::jmax (1, (int)(samplesPerStep * 0.11 * amt));
    return (pseudoRandom01 (salt ^ 0x9e3779b9u) * 2.0f - 1.0f) * (float) maxNudge;
}

bool DrumEngine::rollHitFromProbability (float prob, uint32_t salt) const
{
    float adjusted = std::pow (prob, 1.0f / temperature);
    return pseudoRandom01 (salt) < adjusted;
}

uint8 DrumEngine::sampleVelocityDeterministic (int step, int drum, bool ghost, uint32_t salt) const
{
    const int* range = ghost ? GHOST_VELOCITY_RANGE[style] : STYLE_VELOCITY_RANGE[style];
    int vMin = range[0];
    int vMax = range[1];

    bool isMainBeat = (step % 4 == 0);
    if (isMainBeat && !ghost)
        vMin = (vMin + vMax) / 2;

    const int span = juce::jmax (1, vMax - vMin + 1);
    int vel = vMin + (int)(pseudoRandom01 (salt ^ 0xA53C9E4Du) * (float)span);
    vel = juce::jlimit (vMin, vMax, vel);
    float hum = 0.87f + 0.13f * pseudoRandom01 (salt ^ 0xbeeff00du);
    float stylePocket = 1.0f - 0.09f * STYLE_GROOVE_LOOSENESS[juce::jlimit (0, NUM_STYLES - 1, style)];
    float userPocket  = juce::jmap (hold, 0.88f, 1.0f);
    vel = (int)((float) vel * velocityMul * hum * stylePocket * userPocket);
    return (uint8) juce::jlimit (1, 127, vel);
}

void DrumEngine::evaluateStepForPlayback (int globalStep, int wrappedStep, DrumStep& out,
                                          double samplesPerStep)
{
    wrappedStep = wrappedStep % juce::jmax (1, patternLen);

    if (fillTailPlayback && wrappedStep >= fillTailFromStep)
    {
        out = pattern[wrappedStep];
        if (wrappedStep >= patternLen - 1)
            fillTailPlayback = false;
        return;
    }

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        float base = blendedStyleBase (wrappedStep, drum);
        float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (wrappedStep, drum));
        prob = probabilityAfterDensity (prob, density);

        const uint32_t salt = seed ^ ((uint32_t) globalStep * 2654435761u)
                                    ^ ((uint32_t) drum * 1597334677u);

        bool active = rollHitFromProbability (prob, salt);
        uint8 vel   = 0;
        int   tshift = 0;

        if (active)
        {
            const float ghostThresh = juce::jmap (ghostAmount, 0.42f, 0.10f);
            bool ghost = (base < ghostThresh);
            vel = sampleVelocityDeterministic (wrappedStep, drum, ghost, salt ^ 0x27d4eb2du);

            int maxShift = (int)(samplesPerStep * 0.25 * humanize);
            if (maxShift > 0)
                tshift = (int)(pseudoRandom01 (salt ^ 0x85ebca6bu) * (float)(2 * maxShift + 1)) - maxShift;
            tshift += (int) grooveMicroOffset (wrappedStep, drum, salt, samplesPerStep);
        }

        out[(size_t) drum] = { active, vel, tshift };
    }

    if (fillHoldActive)
    {
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
        {
            float base = blendedStyleBase (wrappedStep, drum);
            const float fillMix = (0.18f + 0.55f * base) * (0.5f + fillRate);
            float prob = probabilityAfterDensity (juce::jlimit (0.0f, 1.0f, fillMix), density);
            if (drum == 1 || drum >= 4) prob *= 1.35f;
            prob = juce::jlimit (0.0f, 1.0f, prob);

            const uint32_t salt = seed ^ ((uint32_t) globalStep * 2246822519u)
                                        ^ ((uint32_t) drum * 3266489917u) ^ 0xF111u;

            if (! rollHitFromProbability (prob, salt))
                continue;

            const float ghostThresh = juce::jmap (ghostAmount, 0.40f, 0.12f);
            bool ghost = (base < ghostThresh);
            uint8 vel = sampleVelocityDeterministic (wrappedStep, drum, ghost, salt ^ 0x27d4eb2du);
            int maxShift = (int)(samplesPerStep * 0.25 * humanize);
            int tshift = 0;
            if (maxShift > 0)
                tshift = (int)(pseudoRandom01 (salt ^ 0x85ebca6bu) * (float)(2 * maxShift + 1)) - maxShift;
            tshift += (int) grooveMicroOffset (wrappedStep, drum, salt, samplesPerStep);

            auto& hit = out[(size_t) drum];
            if (! hit.active || vel > hit.velocity)
                hit = { true, vel, tshift };
        }
    }
}

// ─── Helpers (RNG-based) ─────────────────────────────────────────────────────

float DrumEngine::sampleProb (float baseProbability) const
{
    // Temperature adjusts the sampling distribution:
    //   temp < 1  → more deterministic (high prob events more likely)
    //   temp = 1  → raw probability
    //   temp > 1  → flatter (more random)
    float adjusted = std::pow (baseProbability, 1.0f / temperature);

    std::uniform_real_distribution<float> dist (0.0f, 1.0f);
    return const_cast<DrumEngine*>(this)->rng() / float(std::mt19937::max()) < adjusted;
}

uint8 DrumEngine::sampleVelocity (int step, int drum, bool ghost) const
{
    const int* range = ghost ? GHOST_VELOCITY_RANGE[style] : STYLE_VELOCITY_RANGE[style];
    int vMin = range[0];
    int vMax = range[1];

    // Main beats (0, 4, 8, 12) get fuller velocity
    bool isMainBeat = (step % 4 == 0);
    if (isMainBeat && !ghost)
        vMin = (vMin + vMax) / 2;

    std::uniform_int_distribution<int> dist (vMin, vMax);
    int vel = const_cast<DrumEngine*>(this)->rng() % (vMax - vMin + 1) + vMin;

    // Scale by overall velocity multiplier
    vel = (int)(vel * velocityMul * (1.0f / 1.0f)); // velocityMul already 0–1

    return (uint8)jlimit (1, 127, vel);
}

float DrumEngine::complexityMod (int step, int drum) const
{
    const bool offBeat = (step % 2 != 0);
    const float mod    = (complexity - 0.5f) * 0.5f;
    float weight       = offBeat ? 1.4f : 0.4f;
    if (drum >= 4 && drum <= 7)
        weight *= 1.2f;
    return mod * weight;
}

// Groove invariant: humanize shifts are deterministic from seed + step + drum (no per-block RNG).
void DrumEngine::applyHumanize (double samplesPerStep)
{
    if (humanize < 0.01f) return;

    int maxShift = (int)(samplesPerStep * 0.25 * humanize); // up to 25% of a step
    if (maxShift < 1) return;

    for (int step = 0; step < patternLen; ++step)
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            if (pattern[step][drum].active)
            {
                const uint32_t salt = seed ^ (uint32_t) step * 2246822519u ^ (uint32_t) drum * 3266489917u ^ 0x9E3779B9u;
                const float u = pseudoRandom01 (salt);
                pattern[step][drum].timeShift =
                    (int) std::lround ((u * 2.0f - 1.0f) * (float) maxShift);
            }
}

void DrumEngine::captureMLContext()
{
    static constexpr int mlVoices = 8;
    const int last = patternLen > 0 ? patternLen - 1 : 0;
    for (int v = 0; v < mlVoices; ++v)
        for (int t = 0; t < 4; ++t)
        {
            const int step = juce::jlimit (0, last, last - t);
            const auto& h = pattern[(size_t) step][(size_t) v];
            mlPriorHits[(size_t) (v * 4 + t)] =
                h.active ? juce::jlimit (0.0f, 1.0f, (float) h.velocity / 127.0f) : 0.0f;
        }
}

void DrumEngine::mergePatternFromML (const std::vector<float>& mlOutput)
{
    if (mlOutput.size() < 32)
        return;

    static constexpr int mlVoices = 8;
    const int startMerge = juce::jmax (0, patternLen - 4);
    for (int t = 0; t < 4; ++t)
    {
        const int step = startMerge + t;
        if (step >= patternLen)
            break;

        for (int v = 0; v < mlVoices && v < NUM_DRUMS; ++v)
        {
            const float p = mlOutput[(size_t) (v * 4 + t)];
            auto& hit = pattern[(size_t) step][(size_t) v];

            if (p >= 0.45f)
            {
                if (! hit.active)
                {
                    hit.active = true;
                    hit.velocity = sampleVelocity (step, v, false);
                    hit.timeShift = 0;
                }
                hit.velocity = (uint8) juce::jlimit (
                    1, 127, (int) ((float) hit.velocity * (0.75f + 0.5f * p)));
            }
            else if (p < 0.2f)
            {
                hit.active = false;
                hit.velocity = 0;
                hit.timeShift = 0;
            }
        }
    }
}

int DrumEngine::getSwingOffset (int step, double samplesPerStep) const
{
    const float eff = effectiveSwingAmount();
    if (eff < 0.01f) return 0;

    bool isOffBeat8th = ((step % 4) == 2);
    if (!isOffBeat8th) return 0;

    double swingFraction = 0.5 + (double) eff * 0.17;
    double offset = (swingFraction - 0.5) * samplesPerStep * 2.0;
    return (int) offset;
}

bool DrumEngine::shouldTriggerFill()
{
    ++barCount;
    if (fillRate < 0.01f) return false;

    const float roll = pseudoRandom01 (seed ^ (uint32_t) barCount * 196613u ^ 0xF1110001u);

    const int gridBars = juce::jmax (1, phraseBars);
    const float prob = fillRate * (1.0f + (barCount % gridBars == 0 ? 0.5f : 0.0f));
    static constexpr float cap = 0.32f;
    return roll < prob * cap;
}

void DrumEngine::rebuildGridPreview()
{
    for (int s = 0; s < patternLen; ++s)
    {
        gridPreview[(size_t) s] = pattern[(size_t) s];
        for (int d = 0; d < NUM_DRUMS; ++d)
        {
            auto& h = gridPreview[(size_t) s][(size_t) d];
            if (h.active)
                h.velocity = (uint8) juce::jlimit (1, 127, (int) std::lround ((float) h.velocity * velocityMul));
        }
    }

    for (int s = patternLen; s < NUM_STEPS; ++s)
        for (int d = 0; d < NUM_DRUMS; ++d)
            gridPreview[(size_t) s][(size_t) d] = {};
}

void DrumEngine::morphPatternForDensityAndComplexity (int rangeFromStep0, int rangeToStep0)
{
    if (patternLen < 1)
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

    const int rangeSteps = r1 - r0 + 1;
    const int totalSlots = rangeSteps * NUM_DRUMS;
    const int minTarget  = (density <= 0.001f ? 0
                                               : juce::jmax (0, (int) std::lround ((float) totalSlots * 0.07f)));
    const int maxTarget  = totalSlots;
    int targetActive = (density >= 0.999f ? maxTarget
                                          : (int) std::lround (juce::jmap (density, 0.f, 1.f,
                                                                           (float) minTarget, (float) maxTarget)));
    targetActive = juce::jlimit (0, maxTarget, targetActive);

    auto countActiveInRange = [&] {
        int c = 0;
        for (int s = r0; s <= r1; ++s)
            for (int d = 0; d < NUM_DRUMS; ++d)
                if (pattern[(size_t) s][(size_t) d].active)
                    ++c;
        return c;
    };

    int activeInRange = countActiveInRange();

    while (activeInRange > targetActive)
    {
        int bestS = -1, bestD = -1;
        float bestScore = 1e9f;
        for (int s = r0; s <= r1; ++s)
        {
            for (int d = 0; d < NUM_DRUMS; ++d)
            {
                if (! pattern[(size_t) s][(size_t) d].active)
                    continue;
                const float base = blendedStyleBase (s, d);
                const float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (s, d));
                const float downProt = (s % 4 == 0 && d == 0) ? 0.35f : 0.f;
                const float score = prob + downProt;
                if (score < bestScore)
                {
                    bestScore = score;
                    bestS = s;
                    bestD = d;
                }
            }
        }
        if (bestS < 0)
            break;
        pattern[(size_t) bestS][(size_t) bestD] = {};
        --activeInRange;
    }

    while (activeInRange < targetActive)
    {
        int bestS = -1, bestD = -1;
        float bestScore = -1.f;
        for (int s = r0; s <= r1; ++s)
        {
            for (int d = 0; d < NUM_DRUMS; ++d)
            {
                if (pattern[(size_t) s][(size_t) d].active)
                    continue;
                const float base = blendedStyleBase (s, d);
                const float score = (base + complexityMod (s, d)) * (1.0f + 0.18f * complexity);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestS = s;
                    bestD = d;
                }
            }
        }
        if (bestS < 0)
            break;

        const int s = bestS;
        const int d = bestD;
        const float base = blendedStyleBase (s, d);
        const float ghostThresh = juce::jmap (ghostAmount, 0.42f, 0.10f);
        const bool ghost = (base < ghostThresh);
        pattern[(size_t) s][(size_t) d].active    = true;
        pattern[(size_t) s][(size_t) d].velocity  = sampleVelocity (s, d, ghost);
        pattern[(size_t) s][(size_t) d].timeShift = 0;
        ++activeInRange;
    }

    rebuildGridPreview();
    if (onPatternChanged)
        onPatternChanged();
}

void DrumEngine::adaptPatternToNewStyle (int newStyleIndex)
{
    newStyleIndex = juce::jlimit (0, NUM_STYLES - 1, newStyleIndex);
    style = newStyleIndex;
    if (patternLen < 1)
        return;
    generatePatternRange (0, patternLen - 1, true, nullptr);
}

void DrumEngine::evolvePatternRangeForJam (int fromStep0, int toStep0, BridgeMLManager* ml)
{
    juce::ignoreUnused (ml);
    if (patternLen < 1)
        return;
    fromStep0 = juce::jlimit (0, patternLen - 1, fromStep0);
    toStep0   = juce::jlimit (0, patternLen - 1, toStep0);
    if (toStep0 < fromStep0)
        std::swap (fromStep0, toStep0);

    DrumPattern previous = pattern;
    std::vector<std::tuple<int, int, int>> restoreTimeShift;

    for (int step = fromStep0; step <= toStep0; ++step)
    {
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
        {
            const float t = pseudoRandom01 (seed ^ (uint32_t) step * 2654435761u
                                            ^ (uint32_t) drum * 3266489917u ^ 0xE9010001u);
            if (t < 0.66f && previous[step][drum].active)
            {
                pattern[step][drum] = previous[step][drum];
                restoreTimeShift.emplace_back (step, drum, previous[step][drum].timeShift);
                continue;
            }

            float base = blendedStyleBase (step, drum);
            float prob = juce::jlimit (0.0f, 1.0f, base + complexityMod (step, drum));
            prob = probabilityAfterDensity (prob, density);
            const bool active = sampleProb (prob);

            if (! active)
            {
                pattern[step][drum] = {};
                continue;
            }

            const auto& prev = previous[step][drum];
            pattern[step][drum].active = true;

            if (prev.active)
            {
                pattern[step][drum].velocity  = prev.velocity;
                pattern[step][drum].timeShift = prev.timeShift;
                restoreTimeShift.emplace_back (step, drum, prev.timeShift);
            }
            else
            {
                const float ghostThresh = juce::jmap (ghostAmount, 0.42f, 0.10f);
                const bool ghost = (base < ghostThresh);
                pattern[step][drum].velocity  = sampleVelocity (step, drum, ghost);
                pattern[step][drum].timeShift = 0;
            }
        }
    }

    applyHumanize (playbackSamplesPerStep);
    for (const auto& t : restoreTimeShift)
        pattern[(size_t) std::get<0> (t)][(size_t) std::get<1> (t)].timeShift = std::get<2> (t);

    rebuildGridPreview();
    if (onPatternChanged)
        onPatternChanged();
}
