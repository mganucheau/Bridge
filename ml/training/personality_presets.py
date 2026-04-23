# === Named personality knob vectors (keep in sync with Source/PersonalityPresets.h) ===
# Order: rhythmic_tightness, dynamic_range, timbre_texture, tension_arc, tempo_volatility,
#        emotional_temperature, harmonic_adventurousness, structural_predictability,
#        showmanship, genre_loyalty
from __future__ import annotations

# fmt: off
PERSONALITY_PRESETS: dict[str, list[float]] = {
    # Four-on-the-floor energy, tight grid, predictable song sections, genre-forward.
    "driving_rock": [
        0.9, 0.8, 0.7, 0.6, 0.3, 0.6, 0.3, 0.8, 0.7, 0.2,
    ],
    # Loose time feel, wide dynamics, rich harmony, low genre lock-in for reharm freedom.
    "jazz_trio": [
        0.4, 0.9, 0.3, 0.8, 0.6, 0.8, 0.9, 0.2, 0.4, 0.6,
    ],
    # Hypnotic grid, thin texture, flat arc, extreme predictability, loyal to techno tropes.
    "minimal_techno": [
        0.95, 0.2, 0.5, 0.3, 0.05, 0.2, 0.2, 0.9, 0.3, 0.1,
    ],
    # Vocal-forward: warm dynamics, gentle arc, conservative harmony and structure.
    "singer_songwriter": [
        0.55, 0.75, 0.6, 0.65, 0.25, 0.85, 0.35, 0.75, 0.45, 0.55,
    ],
    # Tight pocket, playful showmanship, syncopation-friendly volatility, mid genre loyalty.
    "funk_groove": [
        0.85, 0.65, 0.55, 0.5, 0.45, 0.7, 0.5, 0.4, 0.85, 0.4,
    ],
    # Soft dynamics, blurred texture, slow tension, low tempo volatility, exploratory harmony.
    "ambient": [
        0.35, 0.85, 0.9, 0.75, 0.15, 0.55, 0.75, 0.35, 0.25, 0.45,
    ],
}
# fmt: on

PRESET_DISPLAY_NAMES: dict[str, str] = {
    "driving_rock": "Driving rock",
    "jazz_trio": "Jazz trio",
    "minimal_techno": "Minimal techno",
    "singer_songwriter": "Singer-songwriter",
    "funk_groove": "Funk groove",
    "ambient": "Ambient",
}
