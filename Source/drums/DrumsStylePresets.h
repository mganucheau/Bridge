#pragma once
#include <array>
#include <string>

// General MIDI drum mapping
static constexpr int NUM_DRUMS   = 9;
static constexpr int NUM_STEPS   = 16;
static constexpr int NUM_STYLES  = 20;

// Eight canonical pattern banks (shared by named styles below)
static constexpr int NUM_PATTERN_BANKS = 8;

static constexpr int DRUM_MIDI_NOTES[NUM_DRUMS] = {
    36,  // Kick
    38,  // Snare
    42,  // Hi-Hat Closed
    46,  // Hi-Hat Open
    43,  // Tom Low
    47,  // Tom Mid
    50,  // Tom High
    49,  // Crash
    51   // Ride
};

static const char* DRUM_NAMES[NUM_DRUMS] = {
    "Kick", "Snare", "HH Cls", "HH Opn",
    "Tom Lo", "Tom Mi", "Tom Hi", "Crash", "Ride"
};

static const char* STYLE_NAMES[NUM_STYLES] = {
    "Funk", "Breakbeat", "Techno", "House", "Rock", "Folk", "Indie", "Cumbia",
    "Garage", "DnB", "Jazz", "Hip-Hop", "Reggae", "Electronic", "Latin", "Blues",
    "Soul", "Disco", "Afrobeat", "Trap"
};

// Maps each UI style → one of eight pattern banks (genre-specific vocabulary)
static constexpr int STYLE_PATTERN_MAP[NUM_STYLES] = {
    3, 2, 5, 5, 0, 1, 0, 6, 5, 2, 1, 2, 4, 5, 6, 7, 3, 5, 6, 2
};

// Hit probability per [bank][drum][step] — values 0.0–1.0
// Steps 0..15 = 16th-note grid within one bar (0=beat1, 4=beat2, 8=beat3, 12=beat4)
static constexpr float STYLE_PATTERN_BANK[NUM_PATTERN_BANKS][NUM_DRUMS][NUM_STEPS] = {

// ─── 0: ROCK ───────────────────────────────────────────────────────────────
{
  // Kick   1e+1 2e+2 3e+3 4e+4
  { 0.95f, 0.05f, 0.08f, 0.05f, 0.05f, 0.05f, 0.28f, 0.05f,
    0.85f, 0.05f, 0.18f, 0.05f, 0.10f, 0.05f, 0.15f, 0.10f },
  // Snare
  { 0.02f, 0.02f, 0.02f, 0.05f, 0.95f, 0.05f, 0.05f, 0.08f,
    0.02f, 0.02f, 0.02f, 0.05f, 0.95f, 0.08f, 0.20f, 0.08f },
  // HH Closed
  { 0.90f, 0.05f, 0.85f, 0.05f, 0.85f, 0.05f, 0.85f, 0.05f,
    0.85f, 0.05f, 0.85f, 0.05f, 0.85f, 0.05f, 0.85f, 0.05f },
  // HH Open
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.15f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.12f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.15f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.15f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.08f, 0.00f, 0.15f, 0.05f, 0.00f },
  // Crash
  { 0.75f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.10f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

// ─── 1: JAZZ ───────────────────────────────────────────────────────────────
{
  // Kick   (sparse, walking)
  { 0.50f, 0.03f, 0.10f, 0.03f, 0.10f, 0.03f, 0.18f, 0.03f,
    0.28f, 0.03f, 0.15f, 0.03f, 0.10f, 0.18f, 0.05f, 0.03f },
  // Snare  (rimshot + ghost)
  { 0.02f, 0.08f, 0.05f, 0.08f, 0.15f, 0.10f, 0.10f, 0.12f,
    0.05f, 0.10f, 0.08f, 0.12f, 0.15f, 0.12f, 0.20f, 0.10f },
  // HH Closed (foot on 2 & 4)
  { 0.05f, 0.05f, 0.05f, 0.05f, 0.80f, 0.05f, 0.05f, 0.05f,
    0.05f, 0.05f, 0.05f, 0.05f, 0.80f, 0.05f, 0.05f, 0.05f },
  // HH Open
  { 0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Tom Lo
  { 0.02f, 0.03f, 0.02f, 0.03f, 0.02f, 0.03f, 0.05f, 0.05f,
    0.02f, 0.03f, 0.02f, 0.05f, 0.05f, 0.05f, 0.08f, 0.05f },
  // Tom Mid
  { 0.02f, 0.03f, 0.02f, 0.03f, 0.02f, 0.03f, 0.05f, 0.05f,
    0.02f, 0.03f, 0.02f, 0.05f, 0.05f, 0.05f, 0.08f, 0.05f },
  // Tom Hi
  { 0.02f, 0.03f, 0.02f, 0.03f, 0.02f, 0.03f, 0.05f, 0.05f,
    0.02f, 0.03f, 0.02f, 0.05f, 0.05f, 0.05f, 0.08f, 0.05f },
  // Crash
  { 0.20f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Ride   (jazz ride pattern: ♩.♪ swing feel)
  { 0.95f, 0.05f, 0.68f, 0.05f, 0.85f, 0.05f, 0.68f, 0.05f,
    0.95f, 0.05f, 0.68f, 0.05f, 0.85f, 0.05f, 0.68f, 0.05f },
},

// ─── 2: HIP-HOP ────────────────────────────────────────────────────────────
{
  // Kick   (heavy, syncopated)
  { 0.95f, 0.05f, 0.05f, 0.25f, 0.05f, 0.20f, 0.30f, 0.05f,
    0.18f, 0.35f, 0.05f, 0.10f, 0.05f, 0.05f, 0.15f, 0.05f },
  // Snare  (2 and 4, ghost notes)
  { 0.02f, 0.12f, 0.05f, 0.10f, 0.90f, 0.08f, 0.05f, 0.05f,
    0.05f, 0.12f, 0.05f, 0.10f, 0.90f, 0.08f, 0.12f, 0.05f },
  // HH Closed (16th note groove)
  { 0.80f, 0.62f, 0.75f, 0.60f, 0.75f, 0.62f, 0.75f, 0.58f,
    0.75f, 0.62f, 0.75f, 0.60f, 0.75f, 0.62f, 0.75f, 0.58f },
  // HH Open
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.20f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.15f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.12f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.12f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.05f, 0.00f, 0.12f, 0.05f, 0.00f },
  // Crash
  { 0.30f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

// ─── 3: FUNK ───────────────────────────────────────────────────────────────
{
  // Kick   (syncopated 16th-note pocket)
  { 0.95f, 0.05f, 0.15f, 0.28f, 0.05f, 0.05f, 0.20f, 0.05f,
    0.15f, 0.28f, 0.05f, 0.05f, 0.05f, 0.20f, 0.30f, 0.10f },
  // Snare  (heavy with ghosts)
  { 0.05f, 0.20f, 0.10f, 0.15f, 0.88f, 0.15f, 0.25f, 0.20f,
    0.15f, 0.28f, 0.10f, 0.18f, 0.88f, 0.18f, 0.30f, 0.20f },
  // HH Closed (tight 16ths)
  { 0.92f, 0.75f, 0.88f, 0.72f, 0.88f, 0.75f, 0.88f, 0.72f,
    0.88f, 0.75f, 0.88f, 0.72f, 0.88f, 0.75f, 0.88f, 0.72f },
  // HH Open
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.00f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.12f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.12f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.08f, 0.00f, 0.12f, 0.05f, 0.00f },
  // Crash
  { 0.60f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.08f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

// ─── 4: REGGAE ─────────────────────────────────────────────────────────────
{
  // Kick   (beat 3 emphasis)
  { 0.30f, 0.05f, 0.05f, 0.05f, 0.10f, 0.05f, 0.05f, 0.05f,
    0.92f, 0.05f, 0.05f, 0.10f, 0.05f, 0.05f, 0.05f, 0.05f },
  // Snare  (2 and 4)
  { 0.05f, 0.05f, 0.05f, 0.05f, 0.85f, 0.05f, 0.05f, 0.05f,
    0.05f, 0.05f, 0.05f, 0.05f, 0.85f, 0.05f, 0.05f, 0.05f },
  // HH Closed (on the beat)
  { 0.60f, 0.05f, 0.60f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f,
    0.60f, 0.05f, 0.60f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f },
  // HH Open (skank on off-beats)
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.70f, 0.05f, 0.70f, 0.05f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.70f, 0.05f, 0.70f, 0.05f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.10f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.10f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.05f, 0.00f, 0.10f, 0.05f, 0.00f },
  // Crash
  { 0.50f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

// ─── 5: ELECTRONIC ─────────────────────────────────────────────────────────
{
  // Kick   (four-on-the-floor)
  { 0.95f, 0.05f, 0.05f, 0.05f, 0.95f, 0.05f, 0.05f, 0.05f,
    0.95f, 0.05f, 0.05f, 0.05f, 0.95f, 0.05f, 0.05f, 0.05f },
  // Snare  (2 and 4 clap)
  { 0.02f, 0.02f, 0.02f, 0.02f, 0.95f, 0.02f, 0.02f, 0.02f,
    0.02f, 0.02f, 0.02f, 0.02f, 0.95f, 0.02f, 0.02f, 0.02f },
  // HH Closed (alternating 8th + 16th)
  { 0.60f, 0.82f, 0.60f, 0.82f, 0.60f, 0.82f, 0.60f, 0.82f,
    0.60f, 0.82f, 0.60f, 0.82f, 0.60f, 0.82f, 0.60f, 0.82f },
  // HH Open (before beats 2 & 4)
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.30f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.25f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.15f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.15f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.05f, 0.00f, 0.15f, 0.05f, 0.00f },
  // Crash
  { 0.60f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.10f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

// ─── 6: LATIN ──────────────────────────────────────────────────────────────
{
  // Kick   (Bossa Nova / Samba)
  { 0.85f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f,
    0.50f, 0.05f, 0.05f, 0.60f, 0.05f, 0.05f, 0.05f, 0.05f },
  // Snare  (cross-stick)
  { 0.05f, 0.20f, 0.20f, 0.20f, 0.60f, 0.20f, 0.20f, 0.20f,
    0.10f, 0.20f, 0.20f, 0.20f, 0.60f, 0.20f, 0.30f, 0.20f },
  // HH Closed
  { 0.70f, 0.05f, 0.05f, 0.05f, 0.70f, 0.05f, 0.05f, 0.05f,
    0.70f, 0.05f, 0.05f, 0.05f, 0.70f, 0.05f, 0.05f, 0.05f },
  // HH Open
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Tom Lo (conga-like)
  { 0.05f, 0.15f, 0.05f, 0.15f, 0.05f, 0.15f, 0.05f, 0.15f,
    0.05f, 0.15f, 0.05f, 0.15f, 0.05f, 0.15f, 0.05f, 0.15f },
  // Tom Mid
  { 0.05f, 0.05f, 0.15f, 0.05f, 0.05f, 0.15f, 0.05f, 0.05f,
    0.15f, 0.05f, 0.05f, 0.15f, 0.05f, 0.05f, 0.15f, 0.05f },
  // Tom Hi
  { 0.10f, 0.05f, 0.05f, 0.10f, 0.05f, 0.05f, 0.10f, 0.05f,
    0.05f, 0.10f, 0.05f, 0.05f, 0.10f, 0.05f, 0.05f, 0.10f },
  // Crash
  { 0.40f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
  // Ride   (driving latin ride)
  { 0.80f, 0.05f, 0.60f, 0.05f, 0.70f, 0.05f, 0.60f, 0.05f,
    0.80f, 0.05f, 0.60f, 0.05f, 0.70f, 0.05f, 0.60f, 0.05f },
},

// ─── 7: BLUES ──────────────────────────────────────────────────────────────
{
  // Kick   (shuffle triplet feel)
  { 0.90f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.28f, 0.05f,
    0.72f, 0.05f, 0.15f, 0.05f, 0.05f, 0.05f, 0.15f, 0.10f },
  // Snare  (strong backbeat)
  { 0.02f, 0.02f, 0.02f, 0.02f, 0.92f, 0.05f, 0.05f, 0.10f,
    0.02f, 0.02f, 0.02f, 0.05f, 0.92f, 0.10f, 0.20f, 0.10f },
  // HH Closed (triplet shuffle: 1 skip 3, 1 skip 3...)
  { 0.90f, 0.05f, 0.05f, 0.85f, 0.05f, 0.05f, 0.85f, 0.05f,
    0.05f, 0.85f, 0.05f, 0.05f, 0.85f, 0.05f, 0.05f, 0.05f },
  // HH Open
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.15f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f },
  // Tom Lo
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.15f },
  // Tom Mid
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.15f, 0.05f },
  // Tom Hi
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.08f, 0.00f, 0.15f, 0.05f, 0.00f },
  // Crash
  { 0.65f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.10f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f },
  // Ride
  { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },
},

}; // end STYLE_PATTERN_BANK

inline float stylePatternAt (int styleIndex, int drum, int step)
{
    int s = styleIndex;
    if (s < 0) s = 0;
    if (s >= NUM_STYLES) s = NUM_STYLES - 1;
    const int bank = STYLE_PATTERN_MAP[(size_t) s];
    return STYLE_PATTERN_BANK[bank][drum][step];
}

// Velocity ranges per style [style][2] = {min, max}
static constexpr int STYLE_VELOCITY_RANGE[NUM_STYLES][2] = {
    { 60, 118 }, { 65, 120 }, { 80, 127 }, { 80, 127 }, { 75, 127 }, { 55, 110 }, { 72, 120 }, { 60, 115 },
    { 80, 127 }, { 65, 120 }, { 55, 110 }, { 65, 120 }, { 70, 115 }, { 80, 127 }, { 60, 115 }, { 65, 112 },
    { 58, 115 }, { 80, 127 }, { 62, 118 }, { 62, 118 }
};

// Ghost note velocity range per style
static constexpr int GHOST_VELOCITY_RANGE[NUM_STYLES][2] = {
    { 18, 42 }, { 22, 48 }, { 30, 55 }, { 28, 52 }, { 25, 50 }, { 20, 45 }, { 22, 48 }, { 20, 45 },
    { 28, 52 }, { 22, 48 }, { 20, 45 }, { 22, 48 }, { 25, 50 }, { 30, 55 }, { 20, 45 }, { 22, 48 },
    { 18, 40 }, { 28, 52 }, { 22, 46 }, { 20, 46 }
};

// Swing bias baked into each style (combined with user Swing + Style mix in engine)
static constexpr float STYLE_DEFAULT_SWING[NUM_STYLES] = {
    0.22f, 0.18f, 0.02f, 0.08f, 0.12f, 0.35f, 0.14f, 0.20f,
    0.16f, 0.08f, 0.55f, 0.22f, 0.06f, 0.04f, 0.14f, 0.48f,
    0.20f, 0.06f, 0.24f, 0.12f
};

// Micro-timing looseness / “human” feel (0 = tight, 1 = loose) — scales with Style mix
static constexpr float STYLE_GROOVE_LOOSENESS[NUM_STYLES] = {
    0.72f, 0.55f, 0.18f, 0.28f, 0.42f, 0.68f, 0.48f, 0.62f,
    0.45f, 0.38f, 0.78f, 0.52f, 0.50f, 0.15f, 0.58f, 0.70f,
    0.65f, 0.22f, 0.75f, 0.48f
};

// Adaptive fill vocabulary: 0 default rock fill, 1 funk pocket, 2 dnb roll, 3 jazz light, 4 latin
static constexpr int STYLE_FILL_FAMILY[NUM_STYLES] = {
    1, 0, 0, 0, 0, 3, 0, 4, 0, 2, 3, 0, 0, 0, 4, 3, 1, 0, 4, 0
};
