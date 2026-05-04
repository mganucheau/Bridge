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
        // Toned down from 0.25f: high density no longer floods sparse style cells.
        prob += (1.0f - base) * base * fill * 0.10f;
    }
    return juce::jmin (0.92f, juce::jlimit (0.0f, 1.0f, prob));
}

/** Max fraction of the range×voice grid that morph / density budget targets (per genre bank). */
static float maxActivityFractionForStyle (int style) noexcept
{
    const int b = STYLE_PATTERN_MAP[juce::jlimit (0, NUM_STYLES - 1, style)];
    return STYLE_BANK_ACTIVITY_CAP[juce::jlimit (0, NUM_PATTERN_BANKS - 1, b)];
}

static int targetActiveCells (float density, int totalSlots, int style) noexcept
{
    if (density <= 0.001f || totalSlots <= 0)
        return 0;
    const float cap = maxActivityFractionForStyle (style);
    const float minF = 0.055f;
    const float t    = std::pow (juce::jlimit (0.f, 1.f, density), 0.7f);
    const float f    = juce::jmap (t, 0.f, 1.f, minF, cap);
    return juce::jlimit (0, totalSlots, (int) std::lround (f * (float) totalSlots));
}

// Per-voice "feel" timing bias as a fraction of one 16th step. Real session
// drummers have voice-specific tendencies: kicks lean slightly forward, snares
// lay back behind the click, hats stay tight, ride/crash sit slightly behind.
// Values scaled by the user's `humanize` knob in the engine.
static constexpr float kVoiceFeelBiasFrac[NUM_DRUMS] = {
    -0.022f,  // 0 Kick  — anticipates
    +0.030f,  // 1 Snare — backbeat lay-back
    +0.005f,  // 2 HH closed — very slight lay-back
    +0.010f,  // 3 HH open
    -0.005f,  // 4 Tom Lo — slight anticipation on rolls
    -0.008f,  // 5 Tom Mi
    -0.012f,  // 6 Tom Hi
    +0.020f,  // 7 Crash
    +0.025f,  // 8 Ride
};

// Per-voice velocity random-multiplier window. Snare and toms get the widest
// variance (where ghost / accent dynamics live), hats stay tighter so the
// 16th-pulse doesn't feel jittery.
static constexpr float kVoiceVelocityVariance[NUM_DRUMS] = {
    0.20f,  // 0 Kick
    0.32f,  // 1 Snare
    0.16f,  // 2 HH closed
    0.20f,  // 3 HH open
    0.28f,  // 4 Tom Lo
    0.28f,  // 5 Tom Mi
    0.28f,  // 6 Tom Hi
    0.18f,  // 7 Crash
    0.22f,  // 8 Ride
};

static int voiceFeelTimingShift (int drum, double samplesPerStep, float humanize) noexcept
{
    const int d = juce::jlimit (0, NUM_DRUMS - 1, drum);
    return (int) std::lround ((double) samplesPerStep
                              * (double) kVoiceFeelBiasFrac[d]
                              * (double) humanize);
}

// Loud notes pull forward, soft notes lay back. This single trick — when paired
// with the per-voice bias above — accounts for most of the audible "in the pocket"
// difference between a quantized programming and a session drummer.
static int velocityFeelTimingShift (int velocity, double samplesPerStep, float humanize) noexcept
{
    const float velNorm = (float) juce::jlimit (1, 127, velocity) / 127.0f;
    return (int) std::lround ((double) samplesPerStep
                              * 0.05
                              * (double) (0.55f - velNorm)
                              * (double) humanize);
}

// Three-tier velocity sampler used by both deterministic and RNG-based paths.
// `tier` 0 = soft / ghost-shadow, 1 = normal middle, 2 = accent. The three
// segments are split into thirds of the supplied [vMin, vMax] range and then
// a uniform pick within the chosen tier. This produces the multi-modal velocity
// distribution real drumming has (clusters at ghost / normal / accent), instead
// of the single tight Gaussian around the midpoint that a flat uniform pick gives.
static int sampleVelocityFromTier (int vMin, int vMax, int tier, float u01) noexcept
{
    vMin = juce::jlimit (1, 127, vMin);
    vMax = juce::jlimit (vMin, 127, vMax);
    tier = juce::jlimit (0, 2, tier);
    const int third = juce::jmax (1, (vMax - vMin) / 3);
    int loSeg, hiSeg;
    switch (tier)
    {
        case 2:  loSeg = vMax - third;          hiSeg = vMax; break;       // accent
        case 1:  loSeg = vMin + third;          hiSeg = vMax - third; break; // normal
        default: loSeg = vMin;                  hiSeg = vMin + third; break; // soft
    }
    if (hiSeg <= loSeg) hiSeg = loSeg + 1;
    const int span = juce::jmax (1, hiSeg - loSeg + 1);
    return juce::jlimit (loSeg, hiSeg, loSeg + (int) (u01 * (float) span));
}

// Pick a velocity tier biased by step position AND voice. Main beats lean
// accent; off-beats stay middle; 16th syncopations lean ghost-shadow. Hats
// stay close to the normal tier so consecutive 16th hats don't ping-pong
// between ghost-shadow and accent (which read as "random velocity"). Snare
// and toms keep the full multi-modal distribution where ghost / normal /
// accent dynamics actually live.
static int pickVelocityTier (int step, int drum, bool ghost, float u01) noexcept
{
    if (ghost) return 0;
    const bool isMainBeat = (step % 4 == 0);
    const bool isUpbeat   = (step % 4 == 2);
    const bool isHat      = (drum == 2 || drum == 3); // HH closed / open
    const bool isCymbal   = (drum == 7 || drum == 8); // crash / ride

    if (isHat)
    {
        // Hats: mostly normal with light accent on main beats; almost never
        // drop to ghost tier (those soft hats read as dropped notes).
        if (isMainBeat) return (u01 < 0.40f) ? 2 : 1;
        if (isUpbeat)   return (u01 < 0.20f) ? 2 : 1;
        return (u01 < 0.08f) ? 2 : 1;
    }
    if (isCymbal)
    {
        // Crash / ride: bias toward accent (cymbals are statement strikes).
        if (isMainBeat) return (u01 < 0.85f) ? 2 : 1;
        return (u01 < 0.55f) ? 2 : 1;
    }
    // Kick / snare / toms — full multi-modal distribution.
    if (isMainBeat) return (u01 < 0.65f) ? 2 : (u01 < 0.95f ? 1 : 0);
    if (isUpbeat)   return (u01 < 0.30f) ? 2 : (u01 < 0.80f ? 1 : 0);
    return (u01 < 0.15f) ? 2 : (u01 < 0.60f ? 1 : 0);
}

// Velocity contour macro: shapes the 16-step velocity envelope after generation. Flat = identity;
// Accent = beats 0/8 stronger and 4/12 medium; Crescendo / Decrescendo ramp evenly across the bar.
static float velocityShapeFactor (int shape, int step, int patternLen)
{
    if (shape <= 0 || patternLen <= 0)
        return 1.0f;
    const float t = (float) step / juce::jmax (1.0f, (float) (patternLen - 1));
    switch (shape)
    {
        case 1: // Accent
        {
            const int s = step % 16;
            if (s == 0 || s == 8)  return 1.18f;
            if (s == 4 || s == 12) return 1.05f;
            return 0.88f;
        }
        case 2: // Crescendo
            return juce::jmap (t, 0.f, 1.f, 0.75f, 1.20f);
        case 3: // Decrescendo
            return juce::jmap (t, 0.f, 1.f, 1.20f, 0.75f);
        default:
            return 1.0f;
    }
}
} // namespace

DrumEngine::DrumEngine()
    : rng (std::random_device{}())
{
    pattern.resize ((size_t) bridge::phrase::kMaxSteps);
    gridPreview.resize ((size_t) bridge::phrase::kMaxSteps);
    mlPersonalityKnobs.fill (0.5f);
    for (auto& step : pattern)
        for (auto& hit : step)
            hit = {};
    for (auto& step : gridPreview)
        for (auto& hit : step)
            hit = {};
    rebuildGridPreview();
}

// ─── Core generation ──────────────────────────────────────────────────────────

void DrumEngine::generatePatternRange (int from0, int to0, bool seamlessPerform, BridgeMLManager* ml)
{
    if (patternLen < 1)
        return;
    from0 = juce::jlimit (0, patternLen - 1, from0);
    to0   = juce::jlimit (0, patternLen - 1, to0);
    if (to0 < from0)
        std::swap (from0, to0);

    const DrumPattern previous = pattern;
    std::vector<std::tuple<int, int, int>> restoreTimeShift;

    auto isLayerLocked = [this] (int drum)
    {
        if (drum == 0)            return layerLocks.kick;
        if (drum == 1)            return layerLocks.snare;
        if (drum == 2 || drum == 3) return layerLocks.hats;
        return layerLocks.perc;
    };

    for (int step = from0; step <= to0; ++step)
    {
        if (step >= patternLen)
            break;

        for (int drum = 0; drum < NUM_DRUMS; ++drum)
        {
            // Sting-style lock: keep this layer's hits intact across regenerate.
            if (isLayerLocked (drum))
            {
                pattern[step][drum] = previous[step][drum];
                if (pattern[step][drum].active)
                    restoreTimeShift.emplace_back (step, drum, previous[step][drum].timeShift);
                continue;
            }

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

    for (int step = patternLen; step < bridge::phrase::kMaxSteps; ++step)
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            pattern[(size_t) step][(size_t) drum].active = false;

    applyVoiceExclusionAndPriority (0, patternLen - 1);
    enforcePerStepPolyphonyInRange (0, patternLen - 1, maxPolyphonyForSettings());

    const bool partialRegenWindow = (from0 > 0 || to0 < patternLen - 1);
    if (! partialRegenWindow && ml != nullptr && ml->hasModel (BridgeMLManager::ModelType::DrumHumanizer))
    {
        const std::array<float, 4> groove {
            juce::jlimit (0.0f, 1.0f, swing),
            juce::jlimit (0.0f, 1.0f, density),
            juce::jlimit (0.0f, 1.0f, velocityMul * 0.85f + fillRate * 0.15f),
            juce::jlimit (0.0f, 1.0f, complexity * 0.65f + humanize * 0.35f),
        };
        for (int q = 0; q < 4; ++q)
        {
            captureMLContextForQuarter (q);
            const auto mlOut = ml->generateDrums (mlPersonalityKnobs, mlPriorHits, groove);
            mergeMLBlockAtQuarter (mlOut, q);
        }
    }

    applyVoiceExclusionAndPriority (0, patternLen - 1);
    enforcePerStepPolyphonyInRange (0, patternLen - 1, maxPolyphonyForSettings());
    applyHumanize (playbackSamplesPerStep);

    // Apply velocity contour macro after humanize so users see and hear the shape.
    if (velShape > 0)
    {
        for (int step = 0; step < patternLen; ++step)
        {
            const float k = velocityShapeFactor (velShape, step, patternLen);
            for (int drum = 0; drum < NUM_DRUMS; ++drum)
                if (pattern[(size_t) step][(size_t) drum].active)
                {
                    const int v = (int) std::lround ((float) pattern[(size_t) step][(size_t) drum].velocity * k);
                    pattern[(size_t) step][(size_t) drum].velocity = (uint8) juce::jlimit (1, 127, v);
                }
        }
    }

    // Articulation: hatOpen biases hits between closed (drum 2) and open (drum 3) hat layers.
    if (hatOpen > 0.01f)
    {
        for (int step = 0; step < patternLen; ++step)
        {
            auto& closed = pattern[(size_t) step][2];
            auto& open   = pattern[(size_t) step][3];
            const uint32_t salt = seed ^ ((uint32_t) step * 0x9E3779B9u) ^ 0xCAFEBABEu;
            if (closed.active && pseudoRandom01 (salt) < hatOpen * 0.6f)
            {
                open = closed;
                closed.active = false;
            }
        }
    }

    for (const auto& t : restoreTimeShift)
        pattern[(size_t) std::get<0> (t)][(size_t) std::get<1> (t)].timeShift = std::get<2> (t);

    if (onPatternChanged)
        onPatternChanged();

    rebuildGridPreview();
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

    // Three-tier distribution (ghost-shadow / normal / accent) instead of a flat
    // uniform pick. Real drumming clusters velocities; uniform-fill of [vMin,vMax]
    // produced the "every red note is the same shade" output.
    const float tierRoll = pseudoRandom01 (salt ^ 0xA53C9E4Du);
    const int tier = pickVelocityTier (step, drum, ghost, tierRoll);
    const float pickRoll = pseudoRandom01 (salt ^ 0x517CC1B7u);
    int vel = sampleVelocityFromTier (vMin, vMax, tier, pickRoll);

    // Per-voice variance window. Snare and toms get a wider random multiplier so
    // ghost / normal / accent dynamics breathe; hats stay tighter.
    const float voiceVar = kVoiceVelocityVariance[juce::jlimit (0, NUM_DRUMS - 1, drum)];
    const float r3 = pseudoRandom01 (salt ^ 0xbeeff00du);
    const float hum = (1.0f - voiceVar * 0.5f) + voiceVar * r3;

    const float stylePocket = 1.0f - 0.09f * STYLE_GROOVE_LOOSENESS[juce::jlimit (0, NUM_STYLES - 1, style)];
    const float userPocket  = juce::jmap (hold, 0.88f, 1.0f);
    vel = (int) std::lround ((float) vel * velocityMul * hum * stylePocket * userPocket);
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

            // Live velocity contour from the per-step macro shape.
            if (velShape > 0)
            {
                const float k = velocityShapeFactor (velShape, wrappedStep, patternLen);
                vel = (uint8) juce::jlimit (1, 127, (int) std::lround ((float) vel * k));
            }

            // "Life" drift: slow LFO modulates velocity per global step. Stable across plays
            // because it's derived from globalStep, not RNG state.
            if (lifeAmount > 0.01f)
            {
                const float phase = std::sin ((float) globalStep * 0.0625f + (float) drum * 0.7f);
                const float lifeK = 1.0f + lifeAmount * 0.18f * phase;
                vel = (uint8) juce::jlimit (1, 127, (int) std::lround ((float) vel * lifeK));
            }

            int humShift = (int)(samplesPerStep * 0.32 * (humanize + lifeAmount * 0.15f));
            if (humShift > 0)
                tshift = (int)(pseudoRandom01 (salt ^ 0x85ebca6bu) * (float)(2 * humShift + 1)) - humShift;
            tshift += voiceFeelTimingShift     (drum, samplesPerStep, humanize);
            tshift += velocityFeelTimingShift  ((int) vel, samplesPerStep, humanize);
            tshift += (int) grooveMicroOffset  (wrappedStep, drum, salt, samplesPerStep);
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
            int maxShift = (int)(samplesPerStep * 0.32 * humanize);
            int tshift = 0;
            if (maxShift > 0)
                tshift = (int)(pseudoRandom01 (salt ^ 0x85ebca6bu) * (float)(2 * maxShift + 1)) - maxShift;
            tshift += voiceFeelTimingShift     (drum, samplesPerStep, humanize);
            tshift += velocityFeelTimingShift  ((int) vel, samplesPerStep, humanize);
            tshift += (int) grooveMicroOffset  (wrappedStep, drum, salt, samplesPerStep);

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

    auto* self = const_cast<DrumEngine*>(this);
    auto u = [&] { return (float) self->rng() / (float) std::mt19937::max(); };

    // Same three-tier distribution as the deterministic playback sampler so the
    // pattern baked at generation time has the same dynamic shape as the live
    // re-evaluated hits.
    const float tierRoll = u();
    const int   tier     = pickVelocityTier (step, drum, ghost, tierRoll);
    int vel = sampleVelocityFromTier (vMin, vMax, tier, u());

    const float voiceVar = kVoiceVelocityVariance[juce::jlimit (0, NUM_DRUMS - 1, drum)];
    const float hum = (1.0f - voiceVar * 0.5f) + voiceVar * u();

    vel = (int) std::lround ((float) vel * velocityMul * hum);
    return (uint8) jlimit (1, 127, vel);
}

float DrumEngine::complexityMod (int step, int drum) const
{
    const bool offBeat = (step % 2 != 0);
    const float mod    = (complexity - 0.5f) * 0.5f;

    // Per-voice complexity sensitivity. Toms and crash should NOT scale with
    // complexity — at max complexity that previously turned a 5% bank value
    // into a 47% chance, carpet-bombing the pattern with random tom/crash hits.
    // Hi-hats own the "complexity" axis (more 16ths, more open accents); kick
    // and snare get a modest boost (more ghosts, more syncopated kicks).
    float voiceWeight = 1.0f;
    switch (drum)
    {
        case 0: voiceWeight = 0.70f; break; // Kick — modest syncopation boost
        case 1: voiceWeight = 0.85f; break; // Snare — ghost-density boost
        case 2: voiceWeight = 1.25f; break; // HH closed — owns complexity
        case 3: voiceWeight = 0.80f; break; // HH open — modest accent boost
        case 4:
        case 5:
        case 6: voiceWeight = 0.15f; break; // Toms — barely scale (fills only)
        case 7: voiceWeight = 0.00f; break; // Crash — never scales (phrase boundary only)
        case 8: voiceWeight = 0.50f; break; // Ride — moderate
        default: break;
    }

    // Position weight: off-beats still get more boost than on-beats (where the
    // bank already places the structural hits), but no longer 1.4× — that was
    // pushing already-loud off-beat probabilities into "fires every bar" land.
    const float positionWeight = offBeat ? 1.10f : 0.50f;
    return mod * voiceWeight * positionWeight;
}

std::array<float, bridge::phrase::kMaxSteps> DrumEngine::getStepActivityGrid() const noexcept
{
    std::array<float, bridge::phrase::kMaxSteps> grid {};
    const int n = juce::jlimit (1, bridge::phrase::kMaxSteps, patternLen);
    for (int step = 0; step < n; ++step)
    {
        float maxV = 0.0f;
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
            if (pattern[(size_t) step][(size_t) drum].active)
                maxV = juce::jmax (maxV, (float) pattern[(size_t) step][(size_t) drum].velocity / 127.0f);
        grid[(size_t) step] = maxV;
    }
    return grid;
}

// Groove invariant: humanize shifts are deterministic from seed + step + drum (no per-block RNG).
// Composition: random scatter ± voice-specific feel bias ± velocity-correlated micro-timing.
void DrumEngine::applyHumanize (double samplesPerStep)
{
    if (humanize < 0.01f) return;

    const int maxShift = (int)(samplesPerStep * 0.32 * humanize); // up to 32% of a step

    for (int step = 0; step < patternLen; ++step)
        for (int drum = 0; drum < NUM_DRUMS; ++drum)
        {
            auto& hit = pattern[step][drum];
            if (! hit.active) continue;

            const uint32_t salt = seed ^ (uint32_t) step * 2246822519u
                                       ^ (uint32_t) drum * 3266489917u ^ 0x9E3779B9u;

            int tshift = 0;
            if (maxShift > 0)
            {
                const float u = pseudoRandom01 (salt);
                tshift = (int) std::lround ((u * 2.0f - 1.0f) * (float) maxShift);
            }
            tshift += voiceFeelTimingShift    (drum, samplesPerStep, humanize);
            tshift += velocityFeelTimingShift ((int) hit.velocity, samplesPerStep, humanize);
            hit.timeShift = tshift;
        }
}

void DrumEngine::captureMLContext()
{
    static constexpr int mlVoices = 8;
    const int            last     = patternLen > 0 ? patternLen - 1 : 0;
    for (int v = 0; v < mlVoices; ++v)
        for (int t = 0; t < 4; ++t)
        {
            const int     step = juce::jlimit (0, last, last - t);
            const auto&   h    = pattern[(size_t) step][(size_t) v];
            mlPriorHits[(size_t) (v * 4 + t)] =
                h.active ? juce::jlimit (0.0f, 1.0f, (float) h.velocity / 127.0f) : 0.0f;
        }
}

void DrumEngine::captureMLContextForQuarter (int quarter0)
{
    static constexpr int mlVoices = 8;
    quarter0 = juce::jlimit (0, 3, quarter0);
    const int qStart = quarter0 * 4;
    for (int v = 0; v < mlVoices; ++v)
        for (int t = 0; t < 4; ++t)
        {
            int step = qStart - 1 - t;
            if (step < 0)
                step += patternLen;
            step = juce::jlimit (0, patternLen - 1, step);
            const auto& h = pattern[(size_t) step][(size_t) v];
            mlPriorHits[(size_t) (v * 4 + t)] =
                h.active ? juce::jlimit (0.0f, 1.0f, (float) h.velocity / 127.0f) : 0.0f;
        }
}

int DrumEngine::maxPolyphonyForSettings() const noexcept
{
    // 3 voices baseline (kick + snare + hat is the canonical max for backbeat),
    // +1 at high complexity (room for an open-hat accent or a tom decoration).
    // Real session drumming clusters at 2-3; 6 produced the "column stack" look.
    return juce::jlimit (3, 4, 3 + (int) std::lround (complexity * 1.0f));
}

void DrumEngine::enforcePerStepPolyphonyInRange (int r0, int r1, int maxVoices)
{
    // Musical drop priority — when over-cap, drop the most disposable voice
    // first and keep the most essential. Crash → ride → toms → HH open →
    // HH closed → snare → kick. (Kick is never dropped if any other voice
    // is also playing at the same step.)
    static constexpr int kDropOrder[NUM_DRUMS] = {
        7,  // Crash
        8,  // Ride
        4, 5, 6, // Toms (lo, mi, hi)
        3,  // HH open
        2,  // HH closed
        1,  // Snare
        0   // Kick (last resort)
    };

    r0 = juce::jlimit (0, patternLen - 1, r0);
    r1 = juce::jlimit (0, patternLen - 1, r1);
    if (r1 < r0)
        std::swap (r0, r1);
    maxVoices = juce::jmax (1, maxVoices);

    for (int s = r0; s <= r1; ++s)
    {
        for (int i = 0; i < NUM_DRUMS; ++i)
        {
            int n = 0;
            for (int d = 0; d < NUM_DRUMS; ++d)
                if (pattern[(size_t) s][(size_t) d].active)
                    ++n;
            if (n <= maxVoices)
                break;

            const int dropD = kDropOrder[i];
            if (pattern[(size_t) s][(size_t) dropD].active)
                pattern[(size_t) s][(size_t) dropD] = {};
        }
    }
}

void DrumEngine::applyVoiceExclusionAndPriority (int r0, int r1)
{
    // Musical exclusion / role rules. Run BEFORE polyphony enforcement so the
    // polyphony pass operates on a pattern that already respects these
    // physical-instrument constraints.
    //
    //   Rule 1 — Crash only on phrase boundary (step 0).
    //            Crashes anywhere else are rolled in by complexity / morph and
    //            are what makes the pattern look like "crashes everywhere".
    //   Rule 2 — Hat mutex: HH Closed and HH Open cannot both fire on the
    //            same step (the open-hat strike replaces the closed strike).
    //   Rule 3 — Crash + HH Closed at the same step: keep Crash (the right
    //            hand can't strike both).
    //   Rule 4 — Multiple toms on one step: keep the tom with the highest
    //            bank base probability and drop the others (a real drummer
    //            can't strike two toms in the same 16th).
    r0 = juce::jlimit (0, patternLen - 1, r0);
    r1 = juce::jlimit (0, patternLen - 1, r1);
    if (r1 < r0)
        std::swap (r0, r1);

    constexpr size_t kKick = 0, kSnare = 1, kHHC = 2, kHHO = 3;
    constexpr size_t kTomLo = 4, kTomMi = 5, kTomHi = 6, kCrash = 7, kRide = 8;
    juce::ignoreUnused (kKick, kSnare, kRide);

    for (int s = r0; s <= r1; ++s)
    {
        auto& step = pattern[(size_t) s];

        // Rule 1: crash only on step 0.
        if (s != 0 && step[kCrash].active)
            step[kCrash] = {};

        // Rule 2: hat mutex — HH Open wins.
        if (step[kHHC].active && step[kHHO].active)
            step[kHHC] = {};

        // Rule 3: crash + HH Closed → drop HH Closed.
        if (step[kCrash].active && step[kHHC].active)
            step[kHHC] = {};

        // Rule 4: multiple toms → keep highest base.
        const bool tomActive[3] = { step[kTomLo].active, step[kTomMi].active, step[kTomHi].active };
        const int  activeTomCount = (tomActive[0] ? 1 : 0)
                                  + (tomActive[1] ? 1 : 0)
                                  + (tomActive[2] ? 1 : 0);
        if (activeTomCount > 1)
        {
            const int tomDrums[3] = { (int) kTomLo, (int) kTomMi, (int) kTomHi };
            int   keepDrum = tomDrums[tomActive[0] ? 0 : (tomActive[1] ? 1 : 2)];
            float keepBase = blendedStyleBase (s, keepDrum);
            for (int i = 0; i < 3; ++i)
            {
                if (! tomActive[i] || tomDrums[i] == keepDrum) continue;
                const float b = blendedStyleBase (s, tomDrums[i]);
                if (b > keepBase) { keepBase = b; keepDrum = tomDrums[i]; }
            }
            for (int i = 0; i < 3; ++i)
                if (tomActive[i] && tomDrums[i] != keepDrum)
                    step[(size_t) tomDrums[i]] = {};
        }
    }
}

void DrumEngine::mergeMLBlockAtQuarter (const std::vector<float>& mlOutput, int quarter0)
{
    if (mlOutput.size() < 32)
        return;

    static constexpr int mlVoices = 8;
    quarter0 = juce::jlimit (0, 3, quarter0);
    for (int t = 0; t < 4; ++t)
    {
        const int step = quarter0 * 4 + t;
        if (step >= patternLen)
            break;

        for (int v = 0; v < mlVoices && v < NUM_DRUMS; ++v)
        {
            const float p = mlOutput[(size_t) (v * 4 + t)];
            auto&       hit = pattern[(size_t) step][(size_t) v];

            if (p >= 0.50f)
            {
                if (! hit.active)
                {
                    hit.active   = true;
                    hit.velocity = sampleVelocity (step, v, false);
                    hit.timeShift = 0;
                }
                hit.velocity = (uint8) juce::jlimit (
                    1, 127, (int) ((float) hit.velocity * (0.78f + 0.45f * p)));
            }
            else if (p < 0.18f)
            {
                hit.active   = false;
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

    for (int s = patternLen; s < bridge::phrase::kMaxSteps; ++s)
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
    int       targetActive = targetActiveCells (density, totalSlots, style);

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

    const int maxP = maxPolyphonyForSettings();
    while (activeInRange < targetActive)
    {
        int bestS = -1, bestD = -1;
        float bestScore = -1.f;
        for (int s = r0; s <= r1; ++s)
        {
            int nOn = 0;
            for (int d = 0; d < NUM_DRUMS; ++d)
                if (pattern[(size_t) s][(size_t) d].active)
                    ++nOn;
            if (nOn >= maxP)
                continue;

            for (int d = 0; d < NUM_DRUMS; ++d)
            {
                if (pattern[(size_t) s][(size_t) d].active)
                    continue;
                const float base = blendedStyleBase (s, d);
                // High complexity: prefer backbeat-adjacent + peripheral layers, not a second linear density ramp.
                const float sync = 1.0f + 0.22f * std::abs (complexity - 0.5f) * 2.0f;
                const float score = (base + complexityMod (s, d)) * sync;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestS     = s;
                    bestD     = d;
                }
            }
        }
        if (bestS < 0)
            break;

        const int   s   = bestS;
        const int   d   = bestD;
        const float base = blendedStyleBase (s, d);
        const float ghostThresh = juce::jmap (ghostAmount, 0.42f, 0.10f);
        const bool  ghost = (base < ghostThresh);
        pattern[(size_t) s][(size_t) d].active   = true;
        pattern[(size_t) s][(size_t) d].velocity  = sampleVelocity (s, d, ghost);
        pattern[(size_t) s][(size_t) d].timeShift = 0;
        ++activeInRange;
    }

    applyVoiceExclusionAndPriority (r0, r1);
    enforcePerStepPolyphonyInRange (r0, r1, maxP);

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
    generatePatternRange (0, patternLen - 1, false, nullptr);
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
