#pragma once
#include <array>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Bootsy — Generative Funk Bass  ·  Style presets & scale data
//
// The "why not stiff" answer lives here:
//   • BASS_NOTE_PROBS      — which 16th steps ring; calibrated from real lines
//   • BASS_PREFERRED_DEGREE— melodic contour per style (root/5th/octave motion)
//   • BASS_GHOST_TENDENCY  — "e" positions are ghost notes, not rests
//   • BASS_TIMING_FEEL     — anticipation / lag per step per style
//   • STYLE_DEFAULT_SWING  — triplet push amount
// ─────────────────────────────────────────────────────────────────────────────

namespace SteviePreset
{

static constexpr int NUM_STEPS   = 16;
static constexpr int NUM_STYLES  = 8;
static constexpr int NUM_DEGREES = 8;   // see degree index table below
static constexpr int NUM_SCALES  = 5;

// ─── Degree index → semitone offset from root ─────────────────────────────────
// Indices used in BASS_PREFERRED_DEGREE:
//   0=Root, 1=m3, 2=P4, 3=P5, 4=m7, 5=Oct(+12), 6=Approach(-1 chromatic), 7=Sub(-12)
static constexpr int DEGREE_SEMITONES[NUM_DEGREES] = {
     0,  // 0  Root
     3,  // 1  Minor 3rd  (dorian / minor feel)
     5,  // 2  Perfect 4th
     7,  // 3  Perfect 5th
    10,  // 4  Minor 7th  (b7 — classic funk chord tone)
    12,  // 5  Octave up
    -1,  // 6  Chromatic approach (engine resolves at generation time)
   -12,  // 7  Sub-octave
};

static const char* DEGREE_NAMES[NUM_DEGREES] = {
    "R", "b3", "4", "5", "b7", "8va", "apr", "sub"
};

// ─── Scale mode index → interval set ─────────────────────────────────────────
// Used for fill note selection and degree resolution when scale != Dorian
static constexpr int SCALE_TONE_COUNT[NUM_SCALES]    = { 7, 5, 6, 5, 7 };
static constexpr int SCALE_INTERVALS[NUM_SCALES][7] = {
    { 0,  2,  3,  5,  7,  9, 10 },   // 0  Dorian          (m7, M6 — funk standard)
    { 0,  3,  5,  7, 10, -1, -1 },   // 1  Minor Pentatonic (5-note, no 2/6)
    { 0,  3,  5,  6,  7, 10, -1 },   // 2  Blues            (b5 blue note)
    { 0,  2,  4,  7,  9, -1, -1 },   // 3  Major Pentatonic (bright)
    { 0,  2,  4,  5,  7,  9, 10 },   // 4  Mixolydian       (b7, M3 — gospel/R&B)
};
static const char* SCALE_NAMES[NUM_SCALES] = {
    "Dorian", "Min Pent", "Blues", "Maj Pent", "Mixolyd"
};

static const char* STYLE_NAMES[NUM_STYLES] = {
    "Ballad", "Soul", "Jazz", "Gospel",
    "Pop", "R&B", "Classical", "Fusion"
};

// ─────────────────────────────────────────────────────────────────────────────
// BASS_NOTE_PROBS[style][step]
//
// Step positions (16th notes inside one bar):
//   Beat:  1    e    +    a    2    e    +    a    3    e    +    a    4    e    +    a
//   Step:  0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
//
// High probability = note almost always fires.
// Low probability  = fires only when density/temperature push it.
// "e" positions (1,5,9,13) have moderate prob because they're ghost notes, not full skips.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float BASS_NOTE_PROBS[NUM_STYLES][NUM_STEPS] = {

// ─── 0: FUNK (Bootsy Collins / James Brown style) ─────────────────────────────
// Strong "1-drop", syncopated "a1" & "+2", ghost "e"s, octave setup on "a4"
{ 0.95f, 0.40f, 0.30f, 0.80f,   0.08f, 0.40f, 0.68f, 0.18f,
  0.88f, 0.35f, 0.22f, 0.72f,   0.10f, 0.40f, 0.64f, 0.85f },

// ─── 1: NEO-SOUL (D'Angelo / Erykah Badu style) ───────────────────────────────
// Flowing melodic walking; lots of in-between chord-tone hits, laid-back feel
{ 0.90f, 0.20f, 0.60f, 0.55f,   0.72f, 0.25f, 0.50f, 0.60f,
  0.82f, 0.20f, 0.50f, 0.58f,   0.68f, 0.28f, 0.42f, 0.55f },

// ─── 2: HIP-HOP (Boom-bap / Trap bass) ───────────────────────────────────────
// Sparse, root-heavy, long sustain — punch on 1 and 3, sub-octave turnaround
{ 0.92f, 0.05f, 0.12f, 0.36f,   0.18f, 0.05f, 0.24f, 0.08f,
  0.85f, 0.05f, 0.12f, 0.32f,   0.14f, 0.05f, 0.22f, 0.40f },

// ─── 3: DISCO (Chic / Moroder style) ─────────────────────────────────────────
// Driving 8th notes, root/octave alternation, upbeat energy
{ 0.95f, 0.22f, 0.90f, 0.25f,   0.92f, 0.22f, 0.88f, 0.28f,
  0.90f, 0.22f, 0.90f, 0.25f,   0.92f, 0.22f, 0.85f, 0.35f },

// ─── 4: AFROBEAT (Fela Kuti / Tony Allen style) ───────────────────────────────
// Repetitive, hypnotic; root & 5th interplay, slight polyrhythmic push
{ 0.88f, 0.28f, 0.62f, 0.50f,   0.18f, 0.40f, 0.60f, 0.24f,
  0.85f, 0.28f, 0.60f, 0.50f,   0.22f, 0.40f, 0.64f, 0.50f },

// ─── 5: R&B (Marvin Gaye / Stevie Wonder style) ───────────────────────────────
// Melodic, warm walking bass — chord tones & chromatic approaches
{ 0.92f, 0.22f, 0.58f, 0.70f,   0.76f, 0.28f, 0.54f, 0.64f,
  0.85f, 0.22f, 0.60f, 0.64f,   0.72f, 0.28f, 0.50f, 0.60f },

// ─── 6: GOSPEL (Kirk Franklin / Hezekiah Walker style) ────────────────────────
// Punchy, powerful; root–5th–octave; strong accent on every downbeat
{ 0.95f, 0.32f, 0.44f, 0.74f,   0.88f, 0.30f, 0.40f, 0.60f,
  0.92f, 0.32f, 0.44f, 0.70f,   0.86f, 0.30f, 0.50f, 0.74f },

// ─── 7: ELEC FUNK (Daft Punk / Prince "Kiss" style) ──────────────────────────
// Mechanical 8th-note grid, synth bass precision, punchy 16th accents
{ 0.92f, 0.05f, 0.88f, 0.12f,   0.88f, 0.05f, 0.88f, 0.24f,
  0.85f, 0.05f, 0.85f, 0.12f,   0.88f, 0.05f, 0.85f, 0.40f },

}; // end BASS_NOTE_PROBS

// ─────────────────────────────────────────────────────────────────────────────
// BASS_PREFERRED_DEGREE[style][step]
//
// Which scale degree the engine prefers at each step.
// "g" comments mark steps that also appear in BASS_GHOST_TENDENCY as high-ghost.
// The engine can deviate from this based on complexity, temperature, and scale.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int BASS_PREFERRED_DEGREE[NUM_STYLES][NUM_STEPS] = {

// 0: FUNK — root on 1, 5th for motion, octave setup on a4
//    step:  0  1g  2   3   4   5g  6   7    8   9g  10  11   12  13g 14  15
{             0,  3,  3,  0,  3,  3,  3,  4,   0,  3,  3,  0,   3,  3,  3,  5 },

// 1: NEO-SOUL — walking chord tones: R→b7→5→b3 contour
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  4,  4,  3,  0,  3,  3,  4,   0,  3,  3,  1,   3,  4,  0,  3 },

// 2: HIP-HOP — root & 5th, sub-octave turnaround
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  0,  3,  0,  3,  0,  0,  0,   0,  0,  3,  0,   3,  0,  0,  7 },

// 3: DISCO — root/octave alternation (classic Chic pattern)
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  5,  5,  3,  0,  5,  5,  3,   0,  5,  5,  3,   0,  5,  5,  3 },

// 4: AFROBEAT — root & 5th in repetitive interlocking groove
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  3,  3,  0,  3,  3,  0,  3,   0,  3,  3,  0,   3,  3,  0,  3 },

// 5: R&B — melodic: R 5 b7 R b3 5 P4 → approach-note movement
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  3,  3,  4,  0,  1,  3,  3,   0,  2,  4,  4,   3,  3,  0,  3 },

// 6: GOSPEL — power: R 5 Oct punching every downbeat
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  3,  3,  0,  5,  3,  0,  3,   0,  3,  3,  0,   5,  3,  0,  3 },

// 7: ELEC FUNK — root/5th machine, b7 for colour on "a" positions
//    step:  0   1   2   3   4   5   6   7    8   9  10  11   12  13  14  15
{             0,  0,  0,  3,  0,  0,  3,  4,   0,  0,  0,  3,   0,  0,  3,  4 },

}; // end BASS_PREFERRED_DEGREE

// ─────────────────────────────────────────────────────────────────────────────
// BASS_GHOST_TENDENCY[style][step]
//
// 0.0 = never a ghost note.  1.0 = always a ghost note when active.
// "e" positions (1,5,9,13) are the classic funk ghost positions.
// Ghost notes are played at ghost-velocity and often with dead articulation.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float BASS_GHOST_TENDENCY[NUM_STYLES][NUM_STEPS] = {

// 0: FUNK — very strong ghost on "e", some on "+"/a
{ 0.0f, 0.92f, 0.18f, 0.22f,   0.0f, 0.92f, 0.18f, 0.28f,
  0.0f, 0.88f, 0.18f, 0.22f,   0.0f, 0.88f, 0.18f, 0.12f },

// 1: NEO-SOUL — softer ghosts, fill notes carry more pitch
{ 0.0f, 0.55f, 0.15f, 0.20f,   0.0f, 0.55f, 0.15f, 0.18f,
  0.0f, 0.50f, 0.15f, 0.20f,   0.0f, 0.55f, 0.15f, 0.14f },

// 2: HIP-HOP — minimal ghosts; mostly hard hits or rests
{ 0.0f, 0.40f, 0.22f, 0.22f,   0.0f, 0.40f, 0.22f, 0.22f,
  0.0f, 0.40f, 0.22f, 0.22f,   0.0f, 0.40f, 0.22f, 0.18f },

// 3: DISCO — almost no ghosts; all clean 8th notes
{ 0.0f, 0.12f, 0.05f, 0.18f,   0.0f, 0.12f, 0.05f, 0.22f,
  0.0f, 0.12f, 0.05f, 0.18f,   0.0f, 0.12f, 0.05f, 0.18f },

// 4: AFROBEAT — moderate ghosts on "e", some on "+"
{ 0.0f, 0.65f, 0.22f, 0.28f,   0.0f, 0.68f, 0.25f, 0.32f,
  0.0f, 0.65f, 0.22f, 0.28f,   0.0f, 0.68f, 0.25f, 0.28f },

// 5: R&B — light ghosts for feel, melodic notes take priority
{ 0.0f, 0.48f, 0.12f, 0.16f,   0.0f, 0.48f, 0.12f, 0.14f,
  0.0f, 0.42f, 0.12f, 0.16f,   0.0f, 0.48f, 0.12f, 0.12f },

// 6: GOSPEL — minimal ghosts; all punchy and intentional
{ 0.0f, 0.28f, 0.10f, 0.14f,   0.0f, 0.28f, 0.10f, 0.14f,
  0.0f, 0.25f, 0.10f, 0.14f,   0.0f, 0.28f, 0.10f, 0.10f },

// 7: ELEC FUNK — no ghosts; quantized, mechanical
{ 0.0f, 0.05f, 0.05f, 0.08f,   0.0f, 0.05f, 0.05f, 0.10f,
  0.0f, 0.05f, 0.05f, 0.08f,   0.0f, 0.05f, 0.05f, 0.10f },

}; // end BASS_GHOST_TENDENCY

// ─────────────────────────────────────────────────────────────────────────────
// BASS_TIMING_FEEL[style][step]
//
// Micro-timing offset tendency per step, in fractions of a 16th note.
// Negative = ahead of grid (anticipation — classic funk move).
// Positive = behind grid  (laid back — soul / hip-hop feel).
// The engine scales this by the "Pocket" parameter (0=tight, 1=full feel).
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float BASS_TIMING_FEEL[NUM_STYLES][NUM_STEPS] = {

// 0: FUNK — "a" beats anticipate; "e" ghost beats slightly late (feel of push-pull)
{  0.00f, +0.04f, -0.05f, -0.12f,   0.00f, +0.04f, -0.06f, -0.08f,
   0.00f, +0.04f, -0.05f, -0.12f,   0.00f, +0.04f, -0.06f, -0.12f },

// 1: NEO-SOUL — uniformly laid back; everything slightly behind
{  0.00f, +0.08f, +0.10f, +0.07f,   0.00f, +0.08f, +0.10f, +0.07f,
   0.00f, +0.08f, +0.10f, +0.07f,   0.00f, +0.08f, +0.10f, +0.07f },

// 2: HIP-HOP — very laid back; sub-bass drags heavy
{  0.00f, +0.10f, +0.12f, +0.10f,   0.00f, +0.10f, +0.12f, +0.10f,
   0.00f, +0.10f, +0.12f, +0.10f,   0.00f, +0.10f, +0.12f, +0.10f },

// 3: DISCO — straight on the grid; metronomic
{  0.00f,  0.00f,  0.00f,  0.00f,   0.00f,  0.00f,  0.00f,  0.00f,
   0.00f,  0.00f,  0.00f,  0.00f,   0.00f,  0.00f,  0.00f,  0.00f },

// 4: AFROBEAT — slight lag on beats, anticipation on "a"
{  0.00f, +0.03f, +0.03f, -0.06f,   0.00f, +0.03f, +0.03f, -0.06f,
   0.00f, +0.03f, +0.03f, -0.06f,   0.00f, +0.03f, +0.03f, -0.06f },

// 5: R&B — warm, gently behind the beat
{  0.00f, +0.05f, +0.07f, +0.05f,   0.00f, +0.05f, +0.07f, +0.05f,
   0.00f, +0.05f, +0.07f, +0.05f,   0.00f, +0.05f, +0.07f, +0.05f },

// 6: GOSPEL — on beat; "a" slightly anticipates for drive
{  0.00f,  0.00f,  0.00f, -0.07f,   0.00f,  0.00f,  0.00f, -0.07f,
   0.00f,  0.00f,  0.00f, -0.07f,   0.00f,  0.00f,  0.00f, -0.07f },

// 7: ELEC FUNK — perfectly quantized; no feel offset
{  0.00f,  0.00f,  0.00f,  0.00f,   0.00f,  0.00f,  0.00f,  0.00f,
   0.00f,  0.00f,  0.00f,  0.00f,   0.00f,  0.00f,  0.00f,  0.00f },

}; // end BASS_TIMING_FEEL

// ─── Velocity ranges ──────────────────────────────────────────────────────────
// Normal note range per style [min, max]
static constexpr int STYLE_VELOCITY_RANGE[NUM_STYLES][2] = {
    { 82, 127 },  // Funk         (wide dynamic range)
    { 65, 112 },  // Neo-Soul     (warm, restrained)
    { 78, 122 },  // Hip-Hop
    { 88, 127 },  // Disco        (punchy, consistent)
    { 72, 118 },  // Afrobeat
    { 65, 110 },  // R&B          (smooth, controlled)
    { 88, 127 },  // Gospel       (very powerful)
    { 90, 127 },  // Elec Funk    (clipped, maximum)
};

// Ghost velocity range per style [min, max]
static constexpr int GHOST_VELOCITY_RANGE[NUM_STYLES][2] = {
    { 15, 42 },  // Funk     (barely there — dead string feel)
    { 25, 52 },  // Neo-Soul
    { 28, 50 },  // Hip-Hop
    { 18, 40 },  // Disco
    { 20, 48 },  // Afrobeat
    { 25, 50 },  // R&B
    { 30, 55 },  // Gospel
    { 25, 45 },  // Elec Funk
};

// Accent boost fraction on main beats (0, 4, 8, 12) — added to velocity
static constexpr float STYLE_ACCENT_STRENGTH[NUM_STYLES] = {
    0.22f,  // Funk         (strong "1-drop")
    0.12f,  // Neo-Soul     (subtle)
    0.16f,  // Hip-Hop
    0.18f,  // Disco
    0.15f,  // Afrobeat
    0.12f,  // R&B
    0.25f,  // Gospel       (very strong)
    0.15f,  // Elec Funk
};

// Default swing per style (0=straight, 1=full triplet swing)
static constexpr float STYLE_DEFAULT_SWING[NUM_STYLES] = {
    0.20f,  // Funk          (slight-to-moderate push)
    0.25f,  // Neo-Soul      (warm swing)
    0.14f,  // Hip-Hop       (MPC swing feel)
    0.05f,  // Disco         (straight 8ths)
    0.10f,  // Afrobeat      (light)
    0.22f,  // R&B           (moderate)
    0.15f,  // Gospel
    0.02f,  // Elec Funk     (quantized)
};

// Note duration as fraction of one 16th-note step, per articulation type:
//   [0] = Normal, [1] = Accent (slightly longer for sustain), [2] = Ghost/dead (short)
// These are further scaled by the Staccato knob in the engine.
static constexpr float ARTICULATION_DURATION[3] = {
    0.80f,  // Normal  — note fills most of the step
    0.90f,  // Accent  — slightly longer, more sustain
    0.35f,  // Ghost   — short, muted, percussive
};

} // namespace SteviePreset
