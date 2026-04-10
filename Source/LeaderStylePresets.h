#pragma once

// Band-wide "Leader" arrangement presets: bias the five leader macros (± range applied before clamp).
static constexpr int NUM_LEADER_STYLES = 8;

static const char* LEADER_STYLE_NAMES[NUM_LEADER_STYLES] = {
    "Balanced",
    "Warehouse Jam",
    "Studio Polish",
    "Late Night",
    "Main Stage",
    "Intimate Set",
    "Fusion Lab",
    "Vintage Revue"
};

struct LeaderStyleBias
{
    float presence; // added to leaderPresence 0..1
    float tight;
    float unity;
    float breath;
    float spark;
};

// Small nudges so the dropdown is felt without fighting the knobs.
static constexpr LeaderStyleBias LEADER_STYLE_BIASES[NUM_LEADER_STYLES] = {
    { 0.00f,  0.00f,  0.00f,  0.00f,  0.00f },
    { -0.04f, -0.14f, -0.10f,  0.10f,  0.14f },
    {  0.08f,  0.14f,  0.10f, -0.06f, -0.08f },
    { -0.06f, -0.08f,  0.06f,  0.12f,  0.04f },
    {  0.06f,  0.06f, -0.04f, -0.10f,  0.12f },
    { -0.08f,  0.10f,  0.12f,  0.14f, -0.10f },
    {  0.04f, -0.06f, -0.12f,  0.04f,  0.16f },
    { -0.02f,  0.08f,  0.08f,  0.06f, -0.06f },
};
