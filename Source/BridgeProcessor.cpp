#include "BridgeProcessor.h"
#include "BridgeTheme.h"
#include "BridgeEditor.h"
#include "BridgeInstrumentStyles.h"
#include "LeaderStylePresets.h"
#include "bass/BassStylePresets.h"
#include "piano/PianoStylePresets.h"
#include "guitar/GuitarStylePresets.h"
#include "ml/BridgeMLManager.h"
#include "BridgeUpdateChecker.h"
#include "PersonalityPresets.h"
#include "BridgePhrase.h"
#include "EnsembleHints.h"
#include <array>
#include <cmath>
#include <vector>

namespace
{
static constexpr const char* kDrumsStateId = "DrumsDrummer";
static constexpr const char* kBassStateId = "Bass";

static int readMelodicStyleEngineIndex (juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("style")))
        return bridgeMelodicEngineStyleIndex (p->getIndex());
    return bridgeMelodicEngineStyleIndex ((int) *apvts.getRawParameterValue ("style"));
}

static int readPhraseBars (juce::AudioProcessorValueTreeState& mainApvts) noexcept
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (mainApvts.getParameter ("phraseBars")))
        return bridge::phrase::phraseBarsFromChoiceIndex (pc->getIndex());
    if (auto* v = mainApvts.getRawParameterValue ("phraseBars"))
        return bridge::phrase::phraseBarsFromChoiceIndex ((int) v->load());
    return bridge::phrase::phraseBarsFromChoiceIndex (0);
}

static int readPhraseSteps (juce::AudioProcessorValueTreeState& mainApvts) noexcept
{
    return bridge::phrase::phraseStepsForBars (readPhraseBars (mainApvts));
}

static bool mainMixerEngaged (juce::AudioProcessorValueTreeState& m)
{
    if (auto* v = m.getRawParameterValue ("leaderTabOn"))
        return v->load() > 0.5f;
    return true;
}

static float readMain01 (juce::AudioProcessorValueTreeState& m, const char* id, float defV)
{
    if (auto* v = m.getRawParameterValue (id))
        return juce::jlimit (0.f, 1.f, v->load());
    return defV;
}

static float readApvts01 (juce::AudioProcessorValueTreeState& ap, const char* id, float defV)
{
    if (auto* v = ap.getRawParameterValue (id))
        return juce::jlimit (0.f, 1.f, v->load());
    return defV;
}

static bool readJamEnabled (juce::AudioProcessorValueTreeState& ap) noexcept
{
    if (auto* v = ap.getRawParameterValue ("jamOn"))
        return v->load() > 0.5f;
    return false;
}

static int readJamPeriodBars (juce::AudioProcessorValueTreeState& ap) noexcept
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("jamPeriodBars")))
        return bridge::phrase::jamPeriodBarsFromChoiceIndex (c->getIndex());
    if (auto* v = ap.getRawParameterValue ("jamPeriodBars"))
        return bridge::phrase::jamPeriodBarsFromChoiceIndex ((int) v->load());
    return 4;
}

/** v15: phrase / jam used 4 choices (norm i/3); remap saved norm to 3-choice indices 0..2 (16 bars → 8). */
static void migrateFourChoiceNormToThree (juce::AudioProcessorValueTreeState& ap,
                                          const juce::String& paramId,
                                          const juce::var& legacyNorm01)
{
    if (legacyNorm01.isVoid())
        return;
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter (paramId));
    if (c == nullptr || c->choices.size() != 3)
        return;
    const float v = juce::jlimit (0.f, 1.f, (float) static_cast<double> (legacyNorm01));
    const int oldIdx = juce::roundToInt (v * 3.f);
    const int newIdx = juce::jlimit (0, 2, oldIdx);
    c->setValueNotifyingHost (c->convertTo0to1 ((float) newIdx));
}

static juce::var findNestedParamValue (const juce::ValueTree& v, const juce::String& paramId)
{
    for (int i = 0; i < v.getNumChildren(); ++i)
    {
        const auto c = v.getChild (i);
        if (c.hasType ("PARAM") && c.getProperty ("id").toString() == paramId)
            return c.getProperty ("value");
        const auto nested = findNestedParamValue (c, paramId);
        if (! nested.isVoid())
            return nested;
    }
    return {};
}

static void apvtsSetFloatFrom01 (juce::AudioProcessorValueTreeState& ap, const char* id, float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (auto* p = ap.getParameter (id))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v01));
}

static void migrateLegacyPerformToJamInterval (juce::AudioProcessorValueTreeState& ap,
                                               const juce::var& legacyPerform)
{
    if (legacyPerform.isVoid())
        return;
    const bool wasOn = (bool) legacyPerform;
    if (! wasOn)
        return;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("jamInterval")))
        c->setValueNotifyingHost (c->convertTo0to1 (3.0f)); // 1 bar
}

static void syncBassMLPersonalityToEngine (juce::AudioProcessorValueTreeState& main, BassEngine& bass)
{
    static constexpr const char* ids[10] = {
        "mlPersRhythmTight",
        "mlPersDynamicRange",
        "mlPersTimbreTexture",
        "mlPersTensionArc",
        "mlPersTempoVolatility",
        "mlPersEmotionalTemp",
        "mlPersHarmAdventure",
        "mlPersStructPredict",
        "mlPersShowmanship",
        "mlPersGenreLoyalty",
    };
    std::vector<float> k (10, 0.5f);
    for (int i = 0; i < 10; ++i)
        k[(size_t) i] = readMain01 (main, ids[i], 0.5f);
    bass.setBassMLPersonalityKnobs (k);
}

static std::array<float, 10> readMainMLPersonalityKnobs10 (juce::AudioProcessorValueTreeState& main)
{
    static constexpr const char* ids[10] = {
        "mlPersRhythmTight",
        "mlPersDynamicRange",
        "mlPersTimbreTexture",
        "mlPersTensionArc",
        "mlPersTempoVolatility",
        "mlPersEmotionalTemp",
        "mlPersHarmAdventure",
        "mlPersStructPredict",
        "mlPersShowmanship",
        "mlPersGenreLoyalty",
    };
    std::array<float, 10> a{};
    for (int i = 0; i < 10; ++i)
        a[(size_t) i] = readMain01 (main, ids[i], 0.5f);
    return a;
}

struct LeaderEffective
{
    float hold = 0.0f;
    float density = 0.5f;
    float swing = 0.0f;
    float humanize = 0.0f;
    float complexity = 0.5f;
};

static LeaderEffective getLeaderEffective (juce::AudioProcessorValueTreeState& m)
{
    int si = 0;
    if (auto* p = m.getParameter ("leaderStyle"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            si = c->getIndex();
    si = juce::jlimit (0, NUM_LEADER_STYLES - 1, si);
    const auto& bias = LEADER_STYLE_BIASES[si];

    auto j = [] (float x) { return juce::jlimit (0.f, 1.f, x); };
    LeaderEffective L;
    L.hold       = j (readMain01 (m, "hold", 0.0f) + bias.hold + bias.tight);
    L.humanize   = j (readMain01 (m, "humanize",  0.0f) + bias.unity);
    L.swing      = j (readMain01 (m, "swing",     0.0f) + bias.breath);
    L.complexity = j (readMain01 (m, "complexity",0.50f) + bias.spark);
    L.density    = readMain01 (m, "density", 0.5f);
    return L;
}

static float leaderMidiGain (juce::AudioProcessorValueTreeState& m)
{
    if (! mainMixerEngaged (m))
        return 1.f;
    const auto L = getLeaderEffective (m);
    return juce::jlimit (0.18f, 1.f, 0.22f + 0.78f * L.density);
}

static void applyLeaderToMelodic (const LeaderEffective& L,
                                  float& temperature,
                                  float& density,
                                  float& swing,
                                  float& humanize,
                                  float& melodicHold,
                                  float& velocity,
                                  float& fillRate,
                                  float& complexity,
                                  float& ghostAmount,
                                  float& staccato)
{
    humanize *= (1.0f - 0.55f * L.hold * 0.45f);
    swing    *= (1.0f - 0.30f * L.hold * 0.45f);
    melodicHold = juce::jmin (1.f, melodicHold + 0.36f * L.hold);
    density  *= (1.0f - 0.48f * L.swing);
    fillRate  = juce::jmin (1.f, fillRate + 0.38f * L.complexity);
    ghostAmount = juce::jmin (1.f, ghostAmount + 0.30f * L.complexity);
    complexity  = juce::jmin (1.f, complexity + 0.22f * L.complexity);
    staccato *= (1.0f - 0.18f * L.humanize);
    temperature = temperature * (1.0f - 0.42f * L.humanize) + 1.0f * (0.42f * L.humanize);
    velocity  = juce::jmin (1.f, velocity * (0.90f + 0.12f * L.humanize));
}

static bool mainRowMidiOpen (juce::AudioProcessorValueTreeState& m,
                             const char* muteId, const char* soloId)
{
    if (! mainMixerEngaged (m))
        return true;
    auto b = [&] (const char* id) -> bool
    {
        return m.getRawParameterValue (id) != nullptr && m.getRawParameterValue (id)->load() > 0.5f;
    };
    const bool anySolo = b ("mainSoloDrums") || b ("mainSoloBass")
                         || b ("mainSoloPiano") || b ("mainSoloGuitar");
    if (anySolo)
        return b (soloId);
    return ! b (muteId);
}

/** Mirrors JUCE's deprecated AudioPlayHead::getCurrentPosition using getPosition(). */
static bool tryGetTransportPosition (juce::AudioPlayHead* playhead,
                                      juce::AudioPlayHead::CurrentPositionInfo& result)
{
    if (playhead == nullptr)
        return false;

    if (const auto pos = playhead->getPosition())
    {
        result.resetToDefault();

        if (const auto sig = pos->getTimeSignature())
        {
            result.timeSigNumerator   = sig->numerator;
            result.timeSigDenominator = sig->denominator;
        }

        if (const auto loop = pos->getLoopPoints())
        {
            result.ppqLoopStart = loop->ppqStart;
            result.ppqLoopEnd   = loop->ppqEnd;
        }

        if (const auto frame = pos->getFrameRate())
            result.frameRate = *frame;

        if (const auto timeInSeconds = pos->getTimeInSeconds())
            result.timeInSeconds = *timeInSeconds;

        if (const auto lastBarStartPpq = pos->getPpqPositionOfLastBarStart())
            result.ppqPositionOfLastBarStart = *lastBarStartPpq;

        if (const auto ppqPosition = pos->getPpqPosition())
            result.ppqPosition = *ppqPosition;

        if (const auto originTime = pos->getEditOriginTime())
            result.editOriginTime = *originTime;

        if (const auto bpm = pos->getBpm())
            result.bpm = *bpm;

        if (const auto timeInSamples = pos->getTimeInSamples())
            result.timeInSamples = *timeInSamples;

        result.isPlaying   = pos->getIsPlaying();
        result.isRecording = pos->getIsRecording();
        result.isLooping   = pos->getIsLooping();

        return true;
    }

    return false;
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout BridgeProcessor::buildMainLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Default startup state: drums soloed; all other lanes muted/off until dialed in.
    layout.add (std::make_unique<juce::AudioParameterBool> ("leaderTabOn", "Leader tab", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("drumsOn",  "Drums on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("bassOn",  "Bass on",  false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("pianoOn",  "Keys on",  false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("guitarOn",    "Guitar on",    false));

    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteDrums", "Mute Drums", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloDrums", "Solo Drums", true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteBass", "Mute Bass", true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloBass", "Solo Bass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMutePiano", "Mute Keys", true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloPiano", "Solo Keys", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteGuitar",   "Mute Guitar",   true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloGuitar",   "Solo Guitar",   false));

    juce::StringArray leaderStyleNames;
    for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        leaderStyleNames.add (LEADER_STYLE_NAMES[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> ("leaderStyle", "Arrangement", leaderStyleNames, 0));

    // Default all groove/expression knobs to minimum until dialed in.
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density", "Density", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing", "Swing", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize", "Humanize", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hold", "Hold", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity", "Velocity", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate", "Fill Rate", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity", "Complexity", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost", 0.f, 1.f, 0.0f));
    // Sting/Session ideas (Phase A–F): per-section macro and section chooser live on main APVTS so
    // every instrument can lerp uniformly. Defaults are neutral so existing presets sound the same.
    layout.add (std::make_unique<juce::AudioParameterChoice> ("arrSection", "Section",
                                                              juce::StringArray { "Intro", "Verse", "Chorus", "Bridge", "Outro" }, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("sectionIntensity", "Section intensity", 0.f, 1.f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart", "Loop Start", 1, bridge::phrase::kMaxSteps, 1));
    // Default phrase is 2 bars → 32 steps; loop end defaults to phrase length.
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd", "Loop End", 1, bridge::phrase::kMaxSteps,
                                                            bridge::phrase::kStepsPerBar * 2));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopOn", "Loop On (legacy)", false));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("playbackLoopOn", "Playback loop", false));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopSpanLock", "Loop span lock", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>("tickerSpeed", "Speed", juce::StringArray { "x2", "1", "1/2" }, 1));
    // Global tonality params
    layout.add (std::make_unique<juce::AudioParameterInt>   ("scale",       "Scale",       0, BassPreset::NUM_SCALES - 1, 2));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("rootNote",    "Root Note",   0, 11, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("octave",      "Octave",      1, 4, 3));

    // Header transport controls
    layout.add (std::make_unique<juce::AudioParameterBool>  ("hostSync",        "Host Sync",         true));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("transportPlaying","Transport Playing", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("internalBpm",     "Internal BPM",
                                                              juce::NormalisableRange<float> (20.0f, 300.0f, 0.1f),
                                                              120.0f));

    juce::StringArray divNames;
    divNames.add ("1/64");
    divNames.add ("1/32");
    divNames.add ("1/24 T");
    divNames.add ("1/16");
    divNames.add ("1/12 T");
    divNames.add ("1/8");
    divNames.add ("1/6 T");
    divNames.add ("1/4");
    divNames.add ("1/2");
    divNames.add ("1 bar");
    layout.add (std::make_unique<juce::AudioParameterChoice> ("timeDivision", "Grid division", divNames, 3));

    // Global phrase length: how many bars the per-instrument grids visualise/loop.
    // Underlying engine patterns remain 1 bar (NUM_STEPS) — generation only fills the loop
    // selection (the "session"); the grid simply tiles that bar across the chosen phrase.
    layout.add (std::make_unique<juce::AudioParameterChoice> ("phraseBars", "Phrase length",
                                                              juce::StringArray { "2 bars", "4 bars", "8 bars" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> ("uiTheme", "UI theme",
                                                              juce::StringArray { "Material Dark" }, 0));

    // ML Personality knobs (0–1) — condition ONNX bass; mapped to MusicVAE latent offsets in training
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersRhythmTight", "Rhythmic tightness", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersDynamicRange", "Dynamic range", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersTimbreTexture", "Timbre texture", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersTensionArc", "Tension arc", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersTempoVolatility", "Tempo volatility", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersEmotionalTemp", "Emotional temperature", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersHarmAdventure", "Harmonic adventurousness", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersStructPredict", "Structural predictability", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersShowmanship", "Showmanship", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mlPersGenreLoyalty", "Genre loyalty", 0.f, 1.f, 0.5f));

    juce::StringArray personalityPresetChoices;
    personalityPresetChoices.add ("Custom");
    for (size_t i = 0; i < bridge::personality::numNamedPresets; ++i)
        personalityPresetChoices.add (bridge::personality::kPresets[i].displayName);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "mlPersonalityPresetName",
        "Personality preset",
        personalityPresetChoices,
        0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        "mlModelsLastUpdateCheckUnix",
        "ML last model update check (unix)",
        0,
        2147000000,
        0));

    return layout;
}

juce::AudioProcessorValueTreeState::ParameterLayout BridgeProcessor::buildDrumsLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Swing, humanize, fill, hold, density/complexity/velocity at 0 (neutral, empty-friendly).

    juce::StringArray styleNames;
    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleNames.add (bridgeUnifiedStyleNames()[i]);

    layout.add (std::make_unique<juce::AudioParameterChoice> ("style", "Style", styleNames, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hold",        "Hold",        0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("intensity",   "Intensity",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> ("midiChannel", "MIDI Channel", 1, 16, 10));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart",   "Loop Start",  1,     NUM_STEPS, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd",     "Loop End",      1,     NUM_STEPS, NUM_STEPS));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopOn", "Loop On", false));
    juce::StringArray jamChoices;
    jamChoices.add ("Off");
    jamChoices.add ("1/4");
    jamChoices.add ("1/2");
    jamChoices.add ("1");
    jamChoices.add ("2");
    jamChoices.add ("4");
    jamChoices.add ("8");
    jamChoices.add ("16");
    layout.add (std::make_unique<juce::AudioParameterChoice> ("jamInterval", "Jam", jamChoices, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> ("jamOn", "Jam on", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("jamPeriodBars", "Jam period",
                                                              juce::StringArray { "2 bars", "4 bars", "8 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("tickerSpeed", "Speed",
                                                              juce::StringArray { "x2", "1", "1/2" }, 1));

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> ("mute_" + juce::String (drum),
                                                                "Mute " + juce::String (DRUM_NAMES[drum]),
                                                                false));
        layout.add (std::make_unique<juce::AudioParameterBool> ("solo_" + juce::String (drum),
                                                                "Solo " + juce::String (DRUM_NAMES[drum]),
                                                                false));
    }

    // Sting-style per-layer locks. Generate copies locked layers from the previous pattern so
    // the drummer can iterate on a single layer (e.g. snare) without losing the rest.
    layout.add (std::make_unique<juce::AudioParameterBool> ("lockKick",  "Lock Kick",  false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("lockSnare", "Lock Snare", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("lockHats",  "Lock Hats",  false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("lockPerc",  "Lock Perc",  false));

    // Continuous "Life" drift (Sting/Session-style aliveness). 0 = static, 1 = constantly breathing.
    // Drives a slow LFO over humanize / velocity at runtime; pattern notes themselves stay stable.
    layout.add (std::make_unique<juce::AudioParameterFloat> ("life", "Life", 0.f, 1.f, 0.0f));

    // Articulation macro for the hat layer (closed -> open balance).
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hatOpen", "Hat Open", 0.f, 1.f, 0.0f));

    // Velocity contour shape for the per-step velocity strip; macro before per-step editing lands.
    layout.add (std::make_unique<juce::AudioParameterChoice> ("velShape", "Vel Shape",
                                                              juce::StringArray { "Flat", "Accent", "Crescendo", "Decrescendo" }, 1));

    return layout;
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeMelodicLayout (int defaultMidiChannel)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Groove / expression defaults: swing, humanize, fill, velocity, hold, staccato at 0 (neutral).

    juce::StringArray styleNames;
    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleNames.add (bridgeUnifiedStyleNames()[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> ("style", "Style", styleNames, 0));
    {
        juce::StringArray jamChoices;
        jamChoices.add ("Off");
        jamChoices.add ("1/4");
        jamChoices.add ("1/2");
        jamChoices.add ("1");
        jamChoices.add ("2");
        jamChoices.add ("4");
        jamChoices.add ("8");
        jamChoices.add ("16");
        layout.add (std::make_unique<juce::AudioParameterChoice> ("jamInterval", "Jam", jamChoices, 0));
    }
    layout.add (std::make_unique<juce::AudioParameterBool> ("jamOn", "Jam on", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("jamPeriodBars", "Jam period",
                                                              juce::StringArray { "2 bars", "4 bars", "8 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("rollSpanOctaves", "Roll span",
                                                              juce::StringArray { "1 oct", "2 oct" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("temperature", "Temperature", 0.01f, 2.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hold",        "Hold",        0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("staccato",    "Staccato",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("sustain",     "Sustain",     0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("intensity",   "Intensity",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("patternLen",  "Pattern Len", 4,     16,   16));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("locked",      "Locked",      false));
    layout.add (std::make_unique<juce::AudioParameterInt> ("midiChannel",  "MIDI Channel", 1, 16, defaultMidiChannel));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("phraseBars", "Bar Grid",
                                                              juce::StringArray { "2 bars", "4 bars", "8 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart",   "Loop Start",   1, BassPreset::NUM_STEPS, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd",     "Loop End",     1, BassPreset::NUM_STEPS, BassPreset::NUM_STEPS));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopOn", "Loop On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("tickerSpeed", "Speed",
                                                              juce::StringArray { "x2", "1", "1/2" }, 1));

    // Continuous "Life" drift; slowly modulates humanize / velocity at runtime so loops breathe.
    layout.add (std::make_unique<juce::AudioParameterFloat> ("life",         "Life",         0.f, 1.f, 0.0f));
    // Continuous melody/motion control: 0 stays anchored on the preferred degree, 1 walks freely.
    layout.add (std::make_unique<juce::AudioParameterFloat> ("melody",       "Melody",       0.f, 1.f, 0.5f));
    // Follow rhythm strength: 0 ignores the drummer, 1 strongly biases onsets toward drum activity.
    layout.add (std::make_unique<juce::AudioParameterFloat> ("followRhythm", "Follow Drums", 0.f, 1.f, 0.0f));

    // Velocity contour shape for the per-step velocity strip.
    layout.add (std::make_unique<juce::AudioParameterChoice> ("velShape", "Vel Shape",
                                                              juce::StringArray { "Flat", "Accent", "Crescendo", "Decrescendo" }, 0));

    // Per-instrument articulation macros surfaced for the engine and MIDI output.
    if (defaultMidiChannel == 1) // Bass
        layout.add (std::make_unique<juce::AudioParameterFloat> ("slideAmt", "Slide", 0.f, 1.f, 0.0f));
    else if (defaultMidiChannel == 2) // Piano
        layout.add (std::make_unique<juce::AudioParameterFloat> ("voicingSpread", "Voicing", 0.f, 1.f, 0.5f));
    else if (defaultMidiChannel == 3) // Guitar
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> ("palmMute",       "Palm Mute", 0.f, 1.f, 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("strumIntensity", "Strum",     0.f, 1.f, 0.5f));
    }

    return layout;
}

BridgeProcessor::BridgeProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Leader MIDI", juce::AudioChannelSet::disabled(), true)
                        .withOutput ("Drums MIDI", juce::AudioChannelSet::disabled(), true)
                        .withOutput ("Bass MIDI", juce::AudioChannelSet::disabled(), true)
                        .withOutput ("Keys MIDI", juce::AudioChannelSet::disabled(), true)
                        .withOutput ("Guitar MIDI", juce::AudioChannelSet::disabled(), true)),
      apvtsMain   (*this, nullptr, "BridgeMain", buildMainLayout()),
      apvtsDrums (*this, nullptr, kDrumsStateId, buildDrumsLayout()),
      apvtsBass (*this, nullptr, kBassStateId, makeMelodicLayout (1)),
      apvtsPiano (*this, nullptr, "Piano", makeMelodicLayout (2)),
      apvtsGuitar   (*this, nullptr, "Guitar", makeMelodicLayout (3))
{
    syncDrumsEngineFromAPVTS();
    syncBassEngineFromAPVTS();
    syncPianoEngineFromAPVTS();
    syncGuitarEngineFromAPVTS();

    drumEngine.onPatternChanged = [this]
    {
        if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
            return;
        juce::MessageManager::callAsync ([this]
        {
            if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
                ed->notifyDrumsPatternChanged();
        });
    };

    bassEngine.onPatternChanged = [this]
    {
        if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
            return;
        juce::MessageManager::callAsync ([this]
        {
            if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
                ed->notifyBassPatternChanged();
        });
    };

    pianoEngine.onPatternChanged = [this]
    {
        if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
            return;
        juce::MessageManager::callAsync ([this]
        {
            if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
                ed->notifyPianoPatternChanged();
        });
    };

    guitarEngine.onPatternChanged = [this]
    {
        if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
            return;
        juce::MessageManager::callAsync ([this]
        {
            if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
                ed->notifyGuitarPatternChanged();
        });
    };

    apvtsMain.addParameterListener ("loopStart", this);
    apvtsMain.addParameterListener ("loopEnd", this);
    apvtsMain.addParameterListener ("loopSpanLock", this);
    apvtsMain.addParameterListener ("scale", this);
    apvtsMain.addParameterListener ("rootNote", this);
    apvtsMain.addParameterListener ("octave", this);
    apvtsMain.addParameterListener ("uiTheme", this);
    apvtsMain.addParameterListener ("arrSection", this);
    apvtsMain.addParameterListener ("sectionIntensity", this);
    apvtsMain.addParameterListener ("phraseBars", this);

    lastSpanLockStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
    lastSpanLockEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("uiTheme")))
        bridge::theme::applyThemeChoiceIndex (p->getIndex());

    mlManager = std::make_unique<BridgeMLManager>();
    mlManager->loadModels();

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        juce::MessageManager::callAsync ([this] { requestAutomaticModelUpdateCheckIfDue(); });
}

BridgeProcessor::~BridgeProcessor()
{
    apvtsMain.removeParameterListener ("loopStart", this);
    apvtsMain.removeParameterListener ("loopEnd", this);
    apvtsMain.removeParameterListener ("loopSpanLock", this);
    apvtsMain.removeParameterListener ("scale", this);
    apvtsMain.removeParameterListener ("rootNote", this);
    apvtsMain.removeParameterListener ("octave", this);
    apvtsMain.removeParameterListener ("arrSection", this);
    apvtsMain.removeParameterListener ("sectionIntensity", this);
    apvtsMain.removeParameterListener ("phraseBars", this);
    apvtsMain.removeParameterListener ("uiTheme", this);
}

juce::AudioProcessorEditor* BridgeProcessor::createEditor()
{
    return new BridgeEditor (*this);
}

void BridgeProcessor::refreshBassKickHintFromDrums()
{
    publishDrumActivityToFollowers();
}

void BridgeProcessor::refreshChordsBassHintFromBass()
{
    std::array<float, 16> hint {};
    const auto& bp = bassEngine.getPattern();
    for (int s = 0; s < 16; ++s)
        hint[(size_t) s] =
            bp[(size_t) s].active
                ? juce::jlimit (0.0f, 1.0f, (float) bp[(size_t) s].velocity / 127.0f)
                : 0.0f;
    pianoEngine.setChordsMLBassHint (hint);
}

void BridgeProcessor::applyArrangementMacro (float& density, float& complexity, bool isDrums) const
{
    int section = 1;
    if (auto* p = const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain).getParameter ("arrSection"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            section = c->getIndex();
    float intensity = 0.5f;
    if (auto* v = const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain).getRawParameterValue ("sectionIntensity"))
        intensity = juce::jlimit (0.f, 1.f, v->load());

    // Section targets: density/complexity offsets at full sectionIntensity (1.0).
    // Intro/Outro pull back, Verse stays neutral, Chorus pushes forward, Bridge sits between.
    struct Target { float dens; float complx; };
    static constexpr Target kDrumTargets[5] = {
        {-0.20f, -0.15f}, // Intro
        {-0.05f, -0.05f}, // Verse
        { 0.20f,  0.15f}, // Chorus
        { 0.05f,  0.10f}, // Bridge
        {-0.30f, -0.20f}, // Outro
    };
    static constexpr Target kMelodicTargets[5] = {
        {-0.15f, -0.20f}, // Intro
        { 0.00f, -0.05f}, // Verse
        { 0.18f,  0.18f}, // Chorus
        { 0.10f,  0.20f}, // Bridge
        {-0.25f, -0.25f}, // Outro
    };
    section = juce::jlimit (0, 4, section);
    const Target t = isDrums ? kDrumTargets[section] : kMelodicTargets[section];
    density    = juce::jlimit (0.f, 1.f, density    + t.dens   * intensity);
    complexity = juce::jlimit (0.f, 1.f, complexity + t.complx * intensity);
}

void BridgeProcessor::publishDrumActivityToFollowers()
{
    const auto snap = bridge::ensemble::snapshotFromDrumEngine (drumEngine);
    bassEngine.setRhythmActivityHint (snap.stepActivity);
    pianoEngine.setRhythmActivityHint (snap.stepActivity);
    guitarEngine.setRhythmActivityHint (snap.stepActivity);
    bassEngine.setBassMLKickHint (snap.kickHint16);
}

void BridgeProcessor::syncDrumsMLPersonalityToEngine()
{
    drumEngine.setMLPersonalityKnobs (readMainMLPersonalityKnobs10 (apvtsMain));
}

void BridgeProcessor::syncChordsMLPersonalityToEngine()
{
    pianoEngine.setMLPersonalityKnobs (readMainMLPersonalityKnobs10 (apvtsMain));
}

void BridgeProcessor::syncMelodyMLPersonalityToEngine()
{
    guitarEngine.setMLPersonalityKnobs (readMainMLPersonalityKnobs10 (apvtsMain));
}

void BridgeProcessor::prepareToPlay (double sr, int spb)
{
    sampleRate = sr;
    samplesPerBlock = spb;

    drumEngine.setHostSampleRate (sr);
    bassEngine.setHostSampleRate (sr);
    pianoEngine.setHostSampleRate (sr);
    guitarEngine.setHostSampleRate (sr);

    midiSampleClock = 0;
    drumsJamDebtBeats = bassJamDebtBeats = pianoJamDebtBeats = guitarJamDebtBeats = 0.0;

    drumsLastProcessedStep = -1;
    drumsPendingNoteOffs.clear();
    drumEngine.setPlaybackSamplesPerStep (sampleRate * 60.0 / 120.0 * getTransportPpqPerPatternStep());
    syncDrumsEngineFromAPVTS();
    drumEngine.rebuildGridPreview();

    bassLastProcessedStep = -1;
    bassPendingNoteOffs.clear();
    syncBassEngineFromAPVTS();
    publishDrumActivityToFollowers();
    bassEngine.rebuildGridPreview();
    refreshChordsBassHintFromBass();

    pianoLastProcessedStep = -1;
    pianoPendingNoteOffs.clear();
    syncPianoEngineFromAPVTS();
    pianoEngine.rebuildGridPreview();

    guitarLastProcessedStep = -1;
    guitarPendingNoteOffs.clear();
    syncGuitarEngineFromAPVTS();
    guitarEngine.rebuildGridPreview();

    refreshClipTimelinesFromEngines();
}

// ONNX / ML paths (e.g. drum humanizer, melodic models) read engine pattern buffers only. After any
// generate or APVTS-driven pattern change, rebuild clips here so playback and BridgeMidiClipEditor
// stay the single timeline view of that same engine state (no separate tensor "adapter" layer yet).
void BridgeProcessor::refreshClipTimelinesFromEngines()
{
    const int phraseSteps = readPhraseSteps (apvtsMain);
    const double bpm      = juce::jmax (1.0, drumsLastBpm > 1.0 ? drumsLastBpm : 120.0);
    const double spb      = sampleRate * 60.0 / bpm;
    const double ppqPerStep = getTransportPpqPerPatternStep();
    const double sps      = spb * ppqPerStep;
    drumsClip.rebuildFromDrumEngine (drumEngine, phraseSteps, sps, spb);
    bassClip.rebuildFromBassEngine (bassEngine, phraseSteps, sps, spb);
    pianoClip.rebuildFromPianoEngine (pianoEngine, phraseSteps, sps, spb);
    guitarClip.rebuildFromGuitarEngine (guitarEngine, phraseSteps, sps, spb);
}

const DrumPattern& BridgeProcessor::getPatternForGrid() const
{
    return drumEngine.getPatternForGrid();
}

bool BridgeProcessor::playbackLoopEngaged() const noexcept
{
    if (apvtsMain.getRawParameterValue ("playbackLoopOn") != nullptr
        && apvtsMain.getRawParameterValue ("playbackLoopOn")->load() > 0.5f)
        return true;
    if (apvtsMain.getRawParameterValue ("loopOn") != nullptr
        && apvtsMain.getRawParameterValue ("loopOn")->load() > 0.5f)
        return true;
    return false;
}

void BridgeProcessor::getMainSelectionBounds (int numSteps, int& loopStart, int& loopEnd) const
{
    loopStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
    loopEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");
    loopStart = juce::jlimit (1, numSteps, loopStart);
    loopEnd   = juce::jlimit (1, numSteps, loopEnd);
    if (loopEnd < loopStart)
        std::swap (loopStart, loopEnd);
}

bool BridgeProcessor::isMainSelectionFullClip (int numSteps) const
{
    int a = 1, b = numSteps;
    getMainSelectionBounds (numSteps, a, b);
    return a == 1 && b == numSteps;
}

void BridgeProcessor::getDrumsPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    if (playbackLoopEngaged())
        getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = phraseSteps;
    }
}

void BridgeProcessor::getBassPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    if (playbackLoopEngaged())
        getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = phraseSteps;
    }
}

void BridgeProcessor::getPianoPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    if (playbackLoopEngaged())
        getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = phraseSteps;
    }
}

void BridgeProcessor::getGuitarPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    if (playbackLoopEngaged())
        getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = phraseSteps;
    }
}

void BridgeProcessor::getDrumsLoopBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
}

void BridgeProcessor::getBassLoopBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
}

double BridgeProcessor::getTransportPpqPerPatternStep() const noexcept
{
    return 0.25; // Always one 16th note per pattern step; timeDivision only affects UI accents.
}

void BridgeProcessor::queueMelodicPreviewNote (int midiChannel, int noteNumber, int velocity)
{
    const juce::ScopedLock sl (previewLock);
    PreviewNoteEvent ev;
    ev.channel = juce::jlimit (1, 16, midiChannel);
    ev.note = juce::jlimit (0, 127, noteNumber);
    ev.velocity = juce::jlimit (1, 127, velocity);
    ev.samplesRemaining = juce::jmax (1, samplesPerBlock);
    ev.noteOnSent = false;
    previewNotes.add (ev);
}

void BridgeProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == "phraseBars")
    {
        const int steps = readPhraseSteps (apvtsMain);
        if (auto* p = apvtsMain.getParameter ("loopEnd"))
            if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
                ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) steps));
        if (auto* p = apvtsMain.getParameter ("loopStart"))
            if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            {
                int ls = (int) *apvtsMain.getRawParameterValue ("loopStart");
                ls = juce::jlimit (1, steps, ls);
                ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) ls));
            }
        lastSpanLockStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
        lastSpanLockEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");

        syncDrumsEngineFromAPVTS();
        syncBassEngineFromAPVTS();
        syncPianoEngineFromAPVTS();
        syncGuitarEngineFromAPVTS();
        drumEngine.rebuildGridPreview();
        bassEngine.rebuildGridPreview();
        pianoEngine.rebuildGridPreview();
        guitarEngine.rebuildGridPreview();
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        {
            ed->notifyDrumsPatternChanged();
            ed->notifyBassPatternChanged();
            ed->notifyPianoPatternChanged();
            ed->notifyGuitarPatternChanged();
            ed->repaint();
        }
        return;
    }

    if (parameterID == "uiTheme")
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("uiTheme")))
            bridge::theme::applyThemeChoiceIndex (p->getIndex());
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->repaint();
        return;
    }

    // Section macro: re-sync engines so per-instrument density/complexity targets glide in.
    if (parameterID == "arrSection" || parameterID == "sectionIntensity")
    {
        syncDrumsEngineFromAPVTS();
        syncBassEngineFromAPVTS();
        syncPianoEngineFromAPVTS();
        syncGuitarEngineFromAPVTS();
        return;
    }

    if (parameterID == "scale" || parameterID == "rootNote" || parameterID == "octave")
    {
        const int oldBassBase = bassEngine.getRootMidiBase();
        const int oldBassScale = bassEngine.getScale();
        const int oldPianoBase = pianoEngine.getRootMidiBase();
        const int oldPianoScale = pianoEngine.getScale();
        const int oldGuitarBase = guitarEngine.getRootMidiBase();
        const int oldGuitarScale = guitarEngine.getScale();

        syncDrumsEngineFromAPVTS();
        syncBassEngineFromAPVTS();
        syncPianoEngineFromAPVTS();
        syncGuitarEngineFromAPVTS();

        bassEngine.remapPatternAfterTonalityChange (oldBassBase, oldBassScale);
        pianoEngine.remapPatternAfterTonalityChange (oldPianoBase, oldPianoScale);
        guitarEngine.remapPatternAfterTonalityChange (oldGuitarBase, oldGuitarScale);

        drumEngine.rebuildGridPreview();
        bassEngine.rebuildGridPreview();
        pianoEngine.rebuildGridPreview();
        guitarEngine.rebuildGridPreview();
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        {
            ed->notifyDrumsPatternChanged();
            ed->notifyBassPatternChanged();
            ed->notifyPianoPatternChanged();
            ed->notifyGuitarPatternChanged();
            ed->repaint();
        }
        if (onMelodicTonalityChanged)
            onMelodicTonalityChanged();
        return;
    }

    if (parameterID != "loopStart" && parameterID != "loopEnd")
    {
        if (parameterID == "loopSpanLock")
        {
            lastSpanLockStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
            lastSpanLockEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");
        }
        return;
    }

    if (spanLockApplying)
        return;

    if (apvtsMain.getRawParameterValue ("loopSpanLock") == nullptr
        || apvtsMain.getRawParameterValue ("loopSpanLock")->load() <= 0.5f)
    {
        lastSpanLockStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
        lastSpanLockEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");
        return;
    }

    const int maxSteps = 16;
    int curS = (int) *apvtsMain.getRawParameterValue ("loopStart");
    int curE = (int) *apvtsMain.getRawParameterValue ("loopEnd");
    curS = juce::jlimit (1, maxSteps, curS);
    curE = juce::jlimit (1, maxSteps, curE);

    const int span = juce::jmax (1, lastSpanLockEnd - lastSpanLockStart + 1);

    auto setIntParam = [] (juce::AudioProcessorValueTreeState& ap, const char* id, int v)
    {
        v = juce::jlimit (1, 16, v);
        if (auto* p = ap.getParameter (id))
            if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            {
                ip->beginChangeGesture();
                ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) v));
                ip->endChangeGesture();
            }
    };

    spanLockApplying = true;

    if (parameterID == "loopStart")
    {
        int newE = curS + span - 1;
        if (newE > maxSteps)
        {
            newE = maxSteps;
            const int newS = juce::jmax (1, newE - span + 1);
            setIntParam (apvtsMain, "loopStart", newS);
            setIntParam (apvtsMain, "loopEnd", newE);
        }
        else
        {
            setIntParam (apvtsMain, "loopEnd", newE);
        }
    }
    else if (parameterID == "loopEnd")
    {
        int newS = curE - span + 1;
        if (newS < 1)
        {
            newS = 1;
            const int newE = juce::jmin (maxSteps, newS + span - 1);
            setIntParam (apvtsMain, "loopStart", newS);
            setIntParam (apvtsMain, "loopEnd", newE);
        }
        else
        {
            setIntParam (apvtsMain, "loopStart", newS);
        }
    }

    spanLockApplying = false;
    lastSpanLockStart = (int) *apvtsMain.getRawParameterValue ("loopStart");
    lastSpanLockEnd   = (int) *apvtsMain.getRawParameterValue ("loopEnd");
}

void BridgeProcessor::syncDrumsEngineFromAPVTS()
{
    int drumStyleIdx;
    if (auto* styleParam = dynamic_cast<juce::AudioParameterChoice*> (apvtsDrums.getParameter ("style")))
        drumStyleIdx = styleParam->getIndex();
    else
        drumStyleIdx = (int) apvtsDrums.getRawParameterValue ("style")->load();
    drumEngine.setStyle (drumStyleIdx);

    {
        const int si = juce::jlimit (0, NUM_STYLES - 1, drumStyleIdx);
        drumEngine.setTemperature (0.65f + STYLE_GROOVE_LOOSENESS[si] * 0.85f);
    }
    float density    = (float)*apvtsDrums.getRawParameterValue ("density");
    float swing      = (float)*apvtsDrums.getRawParameterValue ("swing");
    float humanize   = (float)*apvtsDrums.getRawParameterValue ("humanize");
    float velocity   = (float)*apvtsDrums.getRawParameterValue ("velocity");
    float fillRate   = (float)*apvtsDrums.getRawParameterValue ("fillRate");
    float complexity = (float)*apvtsDrums.getRawParameterValue ("complexity");
    float hold       = (float)*apvtsDrums.getRawParameterValue ("hold");
    float ghostAmt   = (float)*apvtsDrums.getRawParameterValue ("ghostAmount");

    const auto L = getLeaderEffective (apvtsMain);
    const bool allDown =
        density < 0.001f && swing < 0.001f && humanize < 0.001f && velocity < 0.001f
        && fillRate < 0.001f && complexity < 0.001f && hold < 0.001f && ghostAmt < 0.001f;

    if (! allDown)
    {
        humanize *= (1.0f - 0.55f * L.hold * 0.45f);
        swing    *= (1.0f - 0.30f * L.hold * 0.45f);
        hold      = juce::jmin (1.f, hold + 0.38f * L.hold);
        density  *= (1.0f - 0.48f * L.swing);
        fillRate  = juce::jmin (1.f, fillRate + 0.40f * L.complexity);
        ghostAmt  = juce::jmin (1.f, ghostAmt + 0.32f * L.complexity);
        complexity = juce::jmin (1.f, complexity + 0.22f * L.complexity);
        velocity  = juce::jmin (1.f, velocity * (0.90f + 0.14f * L.humanize));

        applyArrangementMacro (density, complexity, /*isDrums*/ true);
    }

    drumEngine.setDensity    (density);
    drumEngine.setSwing      (swing);
    drumEngine.setHumanize   (humanize);
    drumEngine.setVelocity   (velocity);
    drumEngine.setFillRate   (fillRate);
    drumEngine.setComplexity (complexity);
    drumEngine.setHold       (hold);
    drumEngine.setGhostAmount (ghostAmt);
    const int phraseSteps = readPhraseSteps (apvtsMain);
    drumEngine.setPatternLen (phraseSteps);

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsDrums.getParameter ("tickerSpeed")))
        drumsTickerRate = drumsTickerRateForChoiceIndex (tr->getIndex());
    else
        drumsTickerRate = drumsTickerRateForChoiceIndex ((int) *apvtsDrums.getRawParameterValue ("tickerSpeed"));

    drumEngine.setPhraseBars (readPhraseBars (apvtsMain));

    drumsMidiChannel = juce::jlimit (1, 16, (int) *apvtsDrums.getRawParameterValue ("midiChannel"));

    drumsAnySolo = false;
    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        drumsLaneMute[(size_t) drum] = ((bool) *apvtsDrums.getRawParameterValue ("mute_" + juce::String (drum)));
        drumsLaneSolo[(size_t) drum] = ((bool) *apvtsDrums.getRawParameterValue ("solo_" + juce::String (drum)));
        drumsAnySolo = drumsAnySolo || drumsLaneSolo[(size_t) drum];
    }

    // Sting/Session: layer locks, life drift, hat-open articulation, vel-shape macro.
    DrumEngine::LayerMask mask;
    if (auto* p = apvtsDrums.getRawParameterValue ("lockKick"))  mask.kick  = p->load() > 0.5f;
    if (auto* p = apvtsDrums.getRawParameterValue ("lockSnare")) mask.snare = p->load() > 0.5f;
    if (auto* p = apvtsDrums.getRawParameterValue ("lockHats"))  mask.hats  = p->load() > 0.5f;
    if (auto* p = apvtsDrums.getRawParameterValue ("lockPerc"))  mask.perc  = p->load() > 0.5f;
    drumEngine.setLayerLocks (mask);
    drumEngine.setLifeAmount  (readApvts01 (apvtsDrums, "life",    0.f));
    drumEngine.setHatOpen     (readApvts01 (apvtsDrums, "hatOpen", 0.f));
    if (auto* vs = dynamic_cast<juce::AudioParameterChoice*> (apvtsDrums.getParameter ("velShape")))
        drumEngine.setVelShape (vs->getIndex());

    syncDrumsMLPersonalityToEngine();
}

double BridgeProcessor::drumsTickerRateForChoiceIndex (int choiceIndex)
{
    switch (juce::jlimit (0, 2, choiceIndex))
    {
        case 0: return 2.0;
        case 2: return 0.5;
        default: return 1.0;
    }
}

double BridgeProcessor::bassTickerRateForChoiceIndex (int choiceIndex)
{
    switch (juce::jlimit (0, 2, choiceIndex))
    {
        case 0: return 2.0;
        case 2: return 0.5;
        default: return 1.0;
    }
}

bool BridgeProcessor::isDrumsLaneAudible (int drum) const
{
    if (drum < 0 || drum >= NUM_DRUMS) return false;
    return drumsAnySolo ? drumsLaneSolo[(size_t) drum]
                         : ! drumsLaneMute[(size_t) drum];
}

void BridgeProcessor::rebuildDrumsGridPreview()
{
    syncDrumsEngineFromAPVTS();
    drumEngine.rebuildGridPreview();
    refreshClipTimelinesFromEngines();
}

void BridgeProcessor::randomizeDrumsSettings()
{
    juce::Random r;

    if (auto* p = apvtsDrums.getParameter ("style"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (NUM_STYLES)));

    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsDrums.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };

    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("hold",       0.15f, 0.95f);
    randFloat ("ghostAmount",0.15f, 0.95f);

    if (auto* p = apvtsDrums.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));

    drumEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::runDrumGenerateForCurrentMainSelection()
{
    const int phraseSteps = readPhraseSteps (apvtsMain);
    if (isMainSelectionFullClip (phraseSteps))
    {
        drumEngine.generatePattern (false, mlManager.get());
    }
    else
    {
        int a = 1, b = phraseSteps;
        getMainSelectionBounds (phraseSteps, a, b);
        drumEngine.generatePatternRange (a - 1, juce::jmin (b - 1, drumEngine.getPatternLen() - 1), false, mlManager.get());
    }
    publishDrumActivityToFollowers();
    refreshClipTimelinesFromEngines();
}

void BridgeProcessor::regenerateDrumsAfterKnobChange()
{
    syncDrumsEngineFromAPVTS();
    runDrumGenerateForCurrentMainSelection();
}

void BridgeProcessor::triggerDrumsGenerate()
{
    juce::Random r;
    drumEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncDrumsEngineFromAPVTS();
    runDrumGenerateForCurrentMainSelection();
}

void BridgeProcessor::triggerDrumsFill (int fromStep)
{
    const int phraseSteps = readPhraseSteps (apvtsMain);
    int a = 1, b = phraseSteps;
    getMainSelectionBounds (phraseSteps, a, b);
    const int clamped = juce::jlimit (a, b, fromStep);
    drumsFillQueued   = true;
    drumsFillFromStep = clamped;
}

void BridgeProcessor::syncBassEngineFromAPVTS()
{
    bassEngine.setStyle       (readMelodicStyleEngineIndex (apvtsBass));
    bassEngine.setScale       ((int)  *apvtsMain.getRawParameterValue ("scale"));
    bassEngine.setRootNote    ((int)  *apvtsMain.getRawParameterValue ("rootNote"));
    bassEngine.setOctave      ((int)  *apvtsMain.getRawParameterValue ("octave"));
    float temperature = (float)*apvtsBass.getRawParameterValue ("temperature");
    float density     = (float)*apvtsBass.getRawParameterValue ("density");
    float swing       = (float)*apvtsBass.getRawParameterValue ("swing");
    float humanize    = (float)*apvtsBass.getRawParameterValue ("humanize");
    float melodicHold = (float)*apvtsBass.getRawParameterValue ("hold");
    float velocity    = (float)*apvtsBass.getRawParameterValue ("velocity");
    float fillRate    = (float)*apvtsBass.getRawParameterValue ("fillRate");
    float complexity  = (float)*apvtsBass.getRawParameterValue ("complexity");
    float ghostAmount = (float)*apvtsBass.getRawParameterValue ("ghostAmount");
    float staccato    = (float)*apvtsBass.getRawParameterValue ("staccato");
    applyLeaderToMelodic (getLeaderEffective (apvtsMain), temperature, density, swing, humanize,
                          melodicHold, velocity, fillRate, complexity, ghostAmount, staccato);
    applyArrangementMacro (density, complexity, /*isDrums*/ false);
    bassEngine.setTemperature (temperature);
    bassEngine.setDensity     (density);
    bassEngine.setSwing       (swing);
    bassEngine.setHumanize    (humanize);
    bassEngine.setHold        (melodicHold);
    bassEngine.setVelocity    (velocity);
    bassEngine.setFillRate    (fillRate);
    bassEngine.setComplexity  (complexity);
    bassEngine.setGhostAmount (ghostAmount);
    bassEngine.setStaccato    (staccato);
    {
        const int phraseBars  = readPhraseBars (apvtsMain);
        const int phraseSteps = readPhraseSteps (apvtsMain);
        bassEngine.setPhraseBars (phraseBars);
        bassEngine.setPatternLen (phraseSteps);
    }
    bassEngine.setLocked      ((bool) *apvtsBass.getRawParameterValue ("locked"));

    bassMidiChannel = juce::jlimit (1, 16, (int)*apvtsBass.getRawParameterValue ("midiChannel"));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsBass.getParameter ("tickerSpeed")))
        bassTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        bassTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsBass.getRawParameterValue ("tickerSpeed"));

    bassEngine.setLifeAmount   (readApvts01 (apvtsBass, "life",         0.f));
    bassEngine.setMelodyMotion (readApvts01 (apvtsBass, "melody",       0.5f));
    bassEngine.setFollowRhythm (readApvts01 (apvtsBass, "followRhythm", 0.f));
    bassEngine.setSlideAmount  (readApvts01 (apvtsBass, "slideAmt",     0.f));
    if (auto* vs = dynamic_cast<juce::AudioParameterChoice*> (apvtsBass.getParameter ("velShape")))
        bassEngine.setVelShape (vs->getIndex());

    {
        int rollIdx = 0;
        if (auto* rc = dynamic_cast<juce::AudioParameterChoice*> (apvtsBass.getParameter ("rollSpanOctaves")))
            rollIdx = rc->getIndex();
        bassEngine.setRollSpanSemitones (rollIdx >= 1 ? 24 : 12);
    }

    syncBassMLPersonalityToEngine (apvtsMain, bassEngine);
}

void BridgeProcessor::triggerBassGenerate()
{
    juce::Random r;
    bassEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncBassEngineFromAPVTS();
    publishDrumActivityToFollowers();
    const int phraseSteps = readPhraseSteps (apvtsMain);
    if (isMainSelectionFullClip (phraseSteps))
        bassEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = phraseSteps;
        getMainSelectionBounds (phraseSteps, a, b);
        bassEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
    refreshChordsBassHintFromBass();
    refreshClipTimelinesFromEngines();
}

void BridgeProcessor::applyBassStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, bridgeUnifiedStyleCount() - 1, styleIndex);
    if (auto* p = apvtsBass.getParameter ("style"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) styleIndex));
    syncBassEngineFromAPVTS();
    refreshBassKickHintFromDrums();
    bassEngine.adaptPatternToNewStyle (readMelodicStyleEngineIndex (apvtsBass));
    refreshChordsBassHintFromBass();
}

void BridgeProcessor::randomizeBassSettings()
{
    juce::Random r;

    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsBass.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };

    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("hold",       0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);

    if (auto* p = apvtsBass.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));

    bassEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::rebuildBassGridPreview()
{
    markMelodicPreviewFlushIfMessageThread();
    syncBassEngineFromAPVTS();
    bassEngine.rebuildGridPreview();
    refreshClipTimelinesFromEngines();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyBassPatternChanged();
}

void BridgeProcessor::triggerBassFill (int fromStep)
{
    const int phraseSteps = readPhraseSteps (apvtsMain);
    int a = 1, b = phraseSteps;
    getMainSelectionBounds (phraseSteps, a, b);
    bassFillQueued   = true;
    bassFillFromStep = juce::jlimit (a, b, fromStep);
}

void BridgeProcessor::getPianoLoopBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
}

void BridgeProcessor::getGuitarLoopBounds (int& loopStart, int& loopEnd) const
{
    const int phraseSteps = readPhraseSteps (const_cast<juce::AudioProcessorValueTreeState&> (apvtsMain));
    getMainSelectionBounds (phraseSteps, loopStart, loopEnd);
}

void BridgeProcessor::morphAllEnginesToMainSelection()
{
    syncDrumsEngineFromAPVTS();
    syncBassEngineFromAPVTS();
    syncPianoEngineFromAPVTS();
    syncGuitarEngineFromAPVTS();

    runDrumGenerateForCurrentMainSelection();

    int ls = 1, le = 1;
    getMainSelectionBounds (BassPreset::NUM_STEPS, ls, le);
    bassEngine.morphPatternForDensityAndComplexity (
        ls - 1, juce::jmin (le - 1, bassEngine.getPatternLen() - 1));

    getMainSelectionBounds (PianoPreset::NUM_STEPS, ls, le);
    pianoEngine.morphPatternForDensityAndComplexity (
        ls - 1, juce::jmin (le - 1, pianoEngine.getPatternLen() - 1));

    getMainSelectionBounds (GuitarPreset::NUM_STEPS, ls, le);
    guitarEngine.morphPatternForDensityAndComplexity (
        ls - 1, juce::jmin (le - 1, guitarEngine.getPatternLen() - 1));

    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
    {
        ed->notifyDrumsPatternChanged();
        ed->notifyBassPatternChanged();
        ed->notifyPianoPatternChanged();
        ed->notifyGuitarPatternChanged();
    }
}

void BridgeProcessor::syncPianoEngineFromAPVTS()
{
    pianoEngine.setStyle       (readMelodicStyleEngineIndex (apvtsPiano));
    pianoEngine.setScale       ((int)  *apvtsMain.getRawParameterValue ("scale"));
    pianoEngine.setRootNote    ((int)  *apvtsMain.getRawParameterValue ("rootNote"));
    pianoEngine.setOctave      ((int)  *apvtsMain.getRawParameterValue ("octave"));
    float temperature = (float)*apvtsPiano.getRawParameterValue ("temperature");
    float density     = (float)*apvtsPiano.getRawParameterValue ("density");
    float swing       = (float)*apvtsPiano.getRawParameterValue ("swing");
    float humanize    = (float)*apvtsPiano.getRawParameterValue ("humanize");
    float melodicHold = (float)*apvtsPiano.getRawParameterValue ("hold");
    float velocity    = (float)*apvtsPiano.getRawParameterValue ("velocity");
    float fillRate    = (float)*apvtsPiano.getRawParameterValue ("fillRate");
    float complexity  = (float)*apvtsPiano.getRawParameterValue ("complexity");
    float ghostAmount = (float)*apvtsPiano.getRawParameterValue ("ghostAmount");
    float staccato    = (float)*apvtsPiano.getRawParameterValue ("staccato");
    applyLeaderToMelodic (getLeaderEffective (apvtsMain), temperature, density, swing, humanize,
                          melodicHold, velocity, fillRate, complexity, ghostAmount, staccato);
    applyArrangementMacro (density, complexity, /*isDrums*/ false);
    pianoEngine.setTemperature (temperature);
    pianoEngine.setDensity     (density);
    pianoEngine.setSwing       (swing);
    pianoEngine.setHumanize    (humanize);
    pianoEngine.setHold        (melodicHold);
    pianoEngine.setVelocity    (velocity);
    pianoEngine.setFillRate    (fillRate);
    pianoEngine.setComplexity  (complexity);
    pianoEngine.setGhostAmount (ghostAmount);
    pianoEngine.setStaccato    (staccato);
    {
        const int phraseBars  = readPhraseBars (apvtsMain);
        const int phraseSteps = readPhraseSteps (apvtsMain);
        pianoEngine.setPhraseBars (phraseBars);
        pianoEngine.setPatternLen (phraseSteps);
    }
    pianoEngine.setLocked      ((bool) *apvtsPiano.getRawParameterValue ("locked"));

    pianoMidiChannel = juce::jlimit (1, 16, (int)*apvtsPiano.getRawParameterValue ("midiChannel"));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsPiano.getParameter ("tickerSpeed")))
        pianoTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        pianoTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsPiano.getRawParameterValue ("tickerSpeed"));

    pianoEngine.setLifeAmount    (readApvts01 (apvtsPiano, "life",          0.f));
    pianoEngine.setMelodyMotion  (readApvts01 (apvtsPiano, "melody",        0.5f));
    pianoEngine.setFollowRhythm  (readApvts01 (apvtsPiano, "followRhythm",  0.f));
    pianoEngine.setVoicingSpread (readApvts01 (apvtsPiano, "voicingSpread", 0.5f));
    if (auto* vs = dynamic_cast<juce::AudioParameterChoice*> (apvtsPiano.getParameter ("velShape")))
        pianoEngine.setVelShape (vs->getIndex());

    {
        int rollIdx = 0;
        if (auto* rc = dynamic_cast<juce::AudioParameterChoice*> (apvtsPiano.getParameter ("rollSpanOctaves")))
            rollIdx = rc->getIndex();
        pianoEngine.setRollSpanSemitones (rollIdx >= 1 ? 24 : 12);
    }

    syncChordsMLPersonalityToEngine();
}

void BridgeProcessor::syncGuitarEngineFromAPVTS()
{
    guitarEngine.setStyle       (readMelodicStyleEngineIndex (apvtsGuitar));
    guitarEngine.setScale       ((int)  *apvtsMain.getRawParameterValue ("scale"));
    guitarEngine.setRootNote    ((int)  *apvtsMain.getRawParameterValue ("rootNote"));
    guitarEngine.setOctave      ((int)  *apvtsMain.getRawParameterValue ("octave"));
    float temperature = (float)*apvtsGuitar.getRawParameterValue ("temperature");
    float density     = (float)*apvtsGuitar.getRawParameterValue ("density");
    float swing       = (float)*apvtsGuitar.getRawParameterValue ("swing");
    float humanize    = (float)*apvtsGuitar.getRawParameterValue ("humanize");
    float melodicHold = (float)*apvtsGuitar.getRawParameterValue ("hold");
    float velocity    = (float)*apvtsGuitar.getRawParameterValue ("velocity");
    float fillRate    = (float)*apvtsGuitar.getRawParameterValue ("fillRate");
    float complexity  = (float)*apvtsGuitar.getRawParameterValue ("complexity");
    float ghostAmount = (float)*apvtsGuitar.getRawParameterValue ("ghostAmount");
    float staccato    = (float)*apvtsGuitar.getRawParameterValue ("staccato");
    applyLeaderToMelodic (getLeaderEffective (apvtsMain), temperature, density, swing, humanize,
                          melodicHold, velocity, fillRate, complexity, ghostAmount, staccato);
    applyArrangementMacro (density, complexity, /*isDrums*/ false);
    guitarEngine.setTemperature (temperature);
    guitarEngine.setDensity     (density);
    guitarEngine.setSwing       (swing);
    guitarEngine.setHumanize    (humanize);
    guitarEngine.setHold        (melodicHold);
    guitarEngine.setVelocity    (velocity);
    guitarEngine.setFillRate    (fillRate);
    guitarEngine.setComplexity  (complexity);
    guitarEngine.setGhostAmount (ghostAmount);
    guitarEngine.setStaccato    (staccato);
    {
        const int phraseBars  = readPhraseBars (apvtsMain);
        const int phraseSteps = readPhraseSteps (apvtsMain);
        guitarEngine.setPhraseBars (phraseBars);
        guitarEngine.setPatternLen (phraseSteps);
    }
    guitarEngine.setLocked      ((bool) *apvtsGuitar.getRawParameterValue ("locked"));

    guitarMidiChannel = juce::jlimit (1, 16, (int)*apvtsGuitar.getRawParameterValue ("midiChannel"));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsGuitar.getParameter ("tickerSpeed")))
        guitarTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        guitarTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsGuitar.getRawParameterValue ("tickerSpeed"));

    guitarEngine.setLifeAmount     (readApvts01 (apvtsGuitar, "life",           0.f));
    guitarEngine.setMelodyMotion   (readApvts01 (apvtsGuitar, "melody",         0.5f));
    guitarEngine.setFollowRhythm   (readApvts01 (apvtsGuitar, "followRhythm",   0.f));
    guitarEngine.setPalmMute       (readApvts01 (apvtsGuitar, "palmMute",       0.f));
    guitarEngine.setStrumIntensity (readApvts01 (apvtsGuitar, "strumIntensity", 0.5f));
    if (auto* vs = dynamic_cast<juce::AudioParameterChoice*> (apvtsGuitar.getParameter ("velShape")))
        guitarEngine.setVelShape (vs->getIndex());

    {
        int rollIdx = 0;
        if (auto* rc = dynamic_cast<juce::AudioParameterChoice*> (apvtsGuitar.getParameter ("rollSpanOctaves")))
            rollIdx = rc->getIndex();
        guitarEngine.setRollSpanSemitones (rollIdx >= 1 ? 24 : 12);
    }

    syncMelodyMLPersonalityToEngine();
}

void BridgeProcessor::randomizePianoSettings()
{
    juce::Random r;
    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsPiano.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };
    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("hold",       0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);
    if (auto* p = apvtsPiano.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));
    pianoEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::randomizeGuitarSettings()
{
    juce::Random r;
    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsGuitar.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };
    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("hold",       0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);
    if (auto* p = apvtsGuitar.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));
    guitarEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::triggerPianoGenerate()
{
    juce::Random r;
    pianoEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncPianoEngineFromAPVTS();
    refreshChordsBassHintFromBass();
    publishDrumActivityToFollowers();
    const int phraseSteps = readPhraseSteps (apvtsMain);
    if (isMainSelectionFullClip (phraseSteps))
        pianoEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = phraseSteps;
        getMainSelectionBounds (phraseSteps, a, b);
        pianoEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
    refreshClipTimelinesFromEngines();
}

void BridgeProcessor::applyPianoStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, bridgeUnifiedStyleCount() - 1, styleIndex);
    if (auto* p = apvtsPiano.getParameter ("style"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) styleIndex));
    syncPianoEngineFromAPVTS();
    refreshChordsBassHintFromBass();
    pianoEngine.adaptPatternToNewStyle (readMelodicStyleEngineIndex (apvtsPiano));
}

void BridgeProcessor::rebuildPianoGridPreview()
{
    markMelodicPreviewFlushIfMessageThread();
    syncPianoEngineFromAPVTS();
    pianoEngine.rebuildGridPreview();
    refreshClipTimelinesFromEngines();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyPianoPatternChanged();
}

void BridgeProcessor::triggerPianoFill (int fromStep)
{
    const int phraseSteps = readPhraseSteps (apvtsMain);
    int a = 1, b = phraseSteps;
    getMainSelectionBounds (phraseSteps, a, b);
    pianoFillQueued   = true;
    pianoFillFromStep = juce::jlimit (a, b, fromStep);
}

void BridgeProcessor::triggerGuitarGenerate()
{
    juce::Random r;
    guitarEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncGuitarEngineFromAPVTS();
    publishDrumActivityToFollowers();
    const int phraseSteps = readPhraseSteps (apvtsMain);
    if (isMainSelectionFullClip (phraseSteps))
        guitarEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = phraseSteps;
        getMainSelectionBounds (phraseSteps, a, b);
        guitarEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
    refreshClipTimelinesFromEngines();
}

void BridgeProcessor::applyGuitarStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, bridgeUnifiedStyleCount() - 1, styleIndex);
    if (auto* p = apvtsGuitar.getParameter ("style"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) styleIndex));
    syncGuitarEngineFromAPVTS();
    guitarEngine.adaptPatternToNewStyle (readMelodicStyleEngineIndex (apvtsGuitar));
}

void BridgeProcessor::rebuildGuitarGridPreview()
{
    markMelodicPreviewFlushIfMessageThread();
    syncGuitarEngineFromAPVTS();
    guitarEngine.rebuildGridPreview();
    refreshClipTimelinesFromEngines();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyGuitarPatternChanged();
}

void BridgeProcessor::triggerGuitarFill (int fromStep)
{
    int a = 1, b = GuitarPreset::NUM_STEPS;
    getMainSelectionBounds (GuitarPreset::NUM_STEPS, a, b);
    guitarFillQueued   = true;
    guitarFillFromStep = juce::jlimit (a, b, fromStep);
}

void BridgeProcessor::flushPendingNoteOffs (juce::Array<PendingOff>& pending, juce::MidiBuffer& midi)
{
    for (int i = pending.size() - 1; i >= 0; --i)
    {
        auto& off = pending.getReference (i);
        midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), 0);
        pending.remove (i);
    }
}

void BridgeProcessor::markMelodicPreviewFlushIfMessageThread()
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        melodicPreviewFlushPending.store (true);
}

void BridgeProcessor::servicePianoSustainPedal (juce::MidiBuffer& midi, bool phraseBoundary)
{
    juce::ignoreUnused (phraseBoundary);
    const float s = readApvts01 (apvtsPiano, "sustain", 0.f);
    const int ch = juce::jlimit (1, 16, pianoMidiChannel);

    if (s <= 0.0f)
    {
        if (lastPianoSustainCc64Value != 0)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
            lastPianoSustainCc64Value = 0;
        }
        pianoSustainLatchDown = false;
        return;
    }

    const int ccVal = juce::jlimit (1, 127, juce::roundToInt (s * 127.0f));
    if (ccVal != lastPianoSustainCc64Value)
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (ch, 64, ccVal), 0);
        lastPianoSustainCc64Value = ccVal;
        pianoSustainLatchDown = true;
    }
}

void BridgeProcessor::scheduleDrumsNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                                   int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMuteDrums", "mainSoloDrums"))
        return;

    const bool stepWasAnticipated = (globalStep == drumsAnticipatedGlobalStep);
    const uint32_t alreadyFiredMask = stepWasAnticipated ? drumsAnticipatedDrumMask : 0u;

    if (! drumsClip.notes.empty())
    {
        const double ppqPerStep = getTransportPpqPerPatternStep();
        const double eventNominalPpq = (double) globalStep * ppqPerStep;

        for (const auto& n : drumsClip.notes)
        {
            if (n.stepIndex0 != wrappedStep)
                continue;
            const int lane = n.drumLane();
            if (lane < 0 || lane >= NUM_DRUMS)
                continue;
            if (! isDrumsLaneAudible (lane))
                continue;
            if ((alreadyFiredMask & (1u << lane)) != 0u)
                continue;

            const double firePpq = eventNominalPpq + n.microPpq;
            int finalOff = (int) std::floor ((firePpq - transportBlockPpqStart) * transportBlockSamplesPerBeat);
            finalOff = juce::jlimit (0, samplesPerBlock - 1, finalOff);

            const int midiNote = juce::jlimit (1, 127, (int) n.pitch);
            const float g = leaderMidiGain (apvtsMain);
            const int velInt = juce::roundToInt ((float) n.velocity * g);
            const uint8 vel = (uint8) juce::jlimit (1, 127, velInt);

            midi.addEvent (juce::MidiMessage::noteOn (drumsMidiChannel, midiNote, vel), finalOff);

            const int64 gateSamples = juce::jmax ((int64) (sampleRate * 0.05),
                                                  (int64) (n.lengthPpq * transportBlockSamplesPerBeat));
            const int64 offAt = midiSampleClock + finalOff + gateSamples;
            drumsPendingNoteOffs.add ({ drumsMidiChannel, midiNote, offAt });
        }

        if (stepWasAnticipated)
        {
            drumsAnticipatedGlobalStep = INT_MIN;
            drumsAnticipatedDrumMask   = 0u;
        }
        return;
    }

    DrumStep hits;
    drumEngine.evaluateStepForPlayback (globalStep, wrappedStep, hits, samplesPerStep);

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        if (! isDrumsLaneAudible (drum)) continue;
        if (! hits[(size_t) drum].active) continue;
        if ((alreadyFiredMask & (1u << drum)) != 0u) continue;

        int swingOff = drumEngine.getSwingOffset (wrappedStep, samplesPerStep);
        int humanOff = hits[(size_t) drum].timeShift;
        int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        const int midiNote = juce::jlimit (1, 127, (int) DRUM_MIDI_NOTES[drum]);
        float g = leaderMidiGain (apvtsMain);
        const int velInt = juce::roundToInt ((float) hits[(size_t) drum].velocity * g);
        uint8 vel = (uint8) juce::jlimit (1, 127, velInt);

        const int midiChannel = drumsMidiChannel;
        midi.addEvent (juce::MidiMessage::noteOn  (midiChannel, midiNote, vel), finalOff);

        const int64 gateSamples = juce::jmax ((int64) (sampleRate * 0.05),
                                              (int64) (samplesPerStep * 0.18));
        int64 offAt = midiSampleClock + finalOff + gateSamples;
        drumsPendingNoteOffs.add ({ midiChannel, midiNote, offAt });
    }

    if (stepWasAnticipated)
    {
        drumsAnticipatedGlobalStep = INT_MIN;
        drumsAnticipatedDrumMask   = 0u;
    }
}

void BridgeProcessor::sendDrumsNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = drumsPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = drumsPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            drumsPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processDrumsBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncDrumsEngineFromAPVTS();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    drumsLastBpm = bpm;
    currentHostBpm.store (bpm);

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double ppqPerStep = getTransportPpqPerPatternStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;
    drumEngine.setPlaybackSamplesPerStep (samplesPerStep);

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;
    transportBlockPpqStart           = ppqStart;
    transportBlockSamplesPerBeat   = samplesPerBeat;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getDrumsPlaybackWrapBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int) std::floor ((double) globalStep * drumsTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != drumsLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != drumsLastProcessedStep)
        {
            if (drumsFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, drumsFillFromStep);
                drumEngine.generateFill (fillStart);
                drumsFillQueued = false;
            }
            else if (drumEngine.getFillRate() > 0.01f && drumEngine.shouldTriggerFill())
            {
                drumEngine.generateFill (juce::jmin (12, le0));
            }
        }

        drumsCurrentStep.store (wrappedStep);
        drumsCurrentVisualStep.store (visualWrapped);
        drumsLastProcessedStep = globalStep;

        scheduleDrumsNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    // ── Anticipation look-ahead ─────────────────────────────────────────────
    // The voice-feel + velocity-correlated micro-timing in DrumEngine produces
    // negative `timeShift` values (notes that anticipate the grid). When such a
    // step's nominal sample offset is past the block end but its anticipation
    // shift would land within this block, emit it now and remember which drums
    // fired so the next block's main loop skips them.
    {
        const int nextGlobalStep = stepEnd + 1;
        const double nextStepPPQ = (double) nextGlobalStep * ppqPerStep;
        const int nextSampleOffset = (int) std::floor ((nextStepPPQ - ppqStart) * samplesPerBeat);

        // Only consider the next step if its anticipation could plausibly fall
        // inside our window (max anticipation is ~0.32 of a step + voice-feel slop).
        const int maxAnticipationSamples = (int) std::ceil (0.40 * samplesPerStep) + 64;

        uint32_t anticipatedMask = 0u;

        if (nextSampleOffset >= numSamples
            && nextSampleOffset - maxAnticipationSamples < numSamples
            && mainRowMidiOpen (apvtsMain, "mainMuteDrums", "mainSoloDrums"))
        {
            const int mod = loopLen > 0 ? ((nextGlobalStep - ls0) % loopLen + loopLen) % loopLen : 0;
            const int nextWrappedStep = ls0 + mod;

            if (! drumsClip.notes.empty())
            {
                const float g = leaderMidiGain (apvtsMain);
                for (const auto& n : drumsClip.notes)
                {
                    if (n.stepIndex0 != nextWrappedStep)
                        continue;
                    const int lane = n.drumLane();
                    if (lane < 0 || lane >= NUM_DRUMS || ! isDrumsLaneAudible (lane))
                        continue;

                    const double firePpq = (double) nextGlobalStep * ppqPerStep + n.microPpq;
                    const int rawOff = (int) std::floor ((firePpq - ppqStart) * samplesPerBeat);
                    if (rawOff < 0 || rawOff >= numSamples)
                        continue;

                    const int finalOff = juce::jlimit (0, numSamples - 1, rawOff);
                    const int midiNote = juce::jlimit (1, 127, (int) n.pitch);
                    const int velInt   = juce::roundToInt ((float) n.velocity * g);
                    const uint8 vel    = (uint8) juce::jlimit (1, 127, velInt);

                    midiMessages.addEvent (juce::MidiMessage::noteOn (drumsMidiChannel, midiNote, vel), finalOff);

                    const int64 gateSamples = juce::jmax ((int64) (sampleRate * 0.05),
                                                          (int64) (n.lengthPpq * samplesPerBeat));
                    const int64 offAt = midiSampleClock + finalOff + gateSamples;
                    drumsPendingNoteOffs.add ({ drumsMidiChannel, midiNote, offAt });

                    anticipatedMask |= (1u << lane);
                }
            }
            else
            {
                DrumStep nextHits;
                drumEngine.evaluateStepForPlayback (nextGlobalStep, nextWrappedStep, nextHits, samplesPerStep);

                const int swingOff = drumEngine.getSwingOffset (nextWrappedStep, samplesPerStep);
                const float g = leaderMidiGain (apvtsMain);

                for (int drum = 0; drum < NUM_DRUMS; ++drum)
                {
                    if (! isDrumsLaneAudible (drum)) continue;
                    if (! nextHits[(size_t) drum].active) continue;

                    const int humanOff = nextHits[(size_t) drum].timeShift;
                    const int rawOff   = nextSampleOffset + swingOff + humanOff;
                    if (rawOff < 0 || rawOff >= numSamples) continue;

                    const int finalOff = juce::jlimit (0, numSamples - 1, rawOff);
                    const int midiNote = juce::jlimit (1, 127, (int) DRUM_MIDI_NOTES[drum]);
                    const int velInt   = juce::roundToInt ((float) nextHits[(size_t) drum].velocity * g);
                    const uint8 vel    = (uint8) juce::jlimit (1, 127, velInt);

                    const int midiChannel = drumsMidiChannel;
                    midiMessages.addEvent (juce::MidiMessage::noteOn (midiChannel, midiNote, vel), finalOff);

                    const int64 gateSamples = juce::jmax ((int64) (sampleRate * 0.05),
                                                          (int64) (samplesPerStep * 0.18));
                    const int64 offAt = midiSampleClock + finalOff + gateSamples;
                    drumsPendingNoteOffs.add ({ midiChannel, midiNote, offAt });

                    anticipatedMask |= (1u << drum);
                }
            }
        }

        drumsAnticipatedGlobalStep = (anticipatedMask != 0u) ? nextGlobalStep : INT_MIN;
        drumsAnticipatedDrumMask   = anticipatedMask;
    }

    sendDrumsNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::scheduleBassNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                                   int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMuteBass", "mainSoloBass"))
        return;

    if (! bassClip.notes.empty())
    {
        const double ppqPerStep = getTransportPpqPerPatternStep();
        for (const auto& n : bassClip.notes)
        {
            if (n.stepIndex0 != wrappedStep)
                continue;

            const double firePpq = (double) globalStep * ppqPerStep + n.microPpq;
            int finalOff = (int) std::floor ((firePpq - transportBlockPpqStart) * transportBlockSamplesPerBeat);
            finalOff = juce::jlimit (0, samplesPerBlock - 1, finalOff);

            const float g = leaderMidiGain (apvtsMain);
            const int note = juce::jlimit (0, 127, (int) n.pitch);
            const uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) n.velocity * g + 0.5f));

            midi.addEvent (juce::MidiMessage::noteOn (bassMidiChannel, note, vel), finalOff);

            const int durSamples = juce::jmax (1, (int) std::floor (n.lengthPpq * transportBlockSamplesPerBeat));
            const int64 offAt    = midiSampleClock + finalOff + durSamples;
            bassPendingNoteOffs.add ({ bassMidiChannel, note, offAt });
        }
        return;
    }

    const BassHit& hit  = bassEngine.getStep (wrappedStep);
    if (!hit.active) return;

    int swingOff = bassEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        float g = leaderMidiGain (apvtsMain);
    uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) hit.velocity * g + 0.5f));
    int   note   = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (bassMidiChannel, note, vel), finalOff);

    int durSamples = bassEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    bassPendingNoteOffs.add ({ bassMidiChannel, note, offAt });
}

void BridgeProcessor::sendBassNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = bassPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = bassPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            bassPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processBassBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncBassEngineFromAPVTS();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    bassLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double ppqPerStep = getTransportPpqPerPatternStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;
    transportBlockPpqStart         = ppqStart;
    transportBlockSamplesPerBeat = samplesPerBeat;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getBassPlaybackWrapBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * bassTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != bassLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != bassLastProcessedStep)
        {
            if (bassFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, bassFillFromStep);
                bassEngine.generateFill (fillStart);
                bassFillQueued = false;
            }
            else if (bassEngine.shouldTriggerFill())
            {
                bassEngine.generateFill (juce::jmin (12, le0));
            }

            refreshChordsBassHintFromBass();
        }

        bassCurrentStep.store (wrappedStep);
        bassCurrentVisualStep.store (visualWrapped);
        bassLastProcessedStep = globalStep;

        scheduleBassNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendBassNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::schedulePianoNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                                  int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMutePiano", "mainSoloPiano"))
        return;

    if (! pianoClip.notes.empty())
    {
        const double ppqPerStep = getTransportPpqPerPatternStep();
        for (const auto& n : pianoClip.notes)
        {
            if (n.stepIndex0 != wrappedStep)
                continue;

            const double firePpq = (double) globalStep * ppqPerStep + n.microPpq;
            int finalOff = (int) std::floor ((firePpq - transportBlockPpqStart) * transportBlockSamplesPerBeat);
            finalOff = juce::jlimit (0, samplesPerBlock - 1, finalOff);

            const float g = leaderMidiGain (apvtsMain);
            const int note = juce::jlimit (0, 127, (int) n.pitch);
            const uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) n.velocity * g + 0.5f));

            midi.addEvent (juce::MidiMessage::noteOn (pianoMidiChannel, note, vel), finalOff);

            const int durSamples = juce::jmax (1, (int) std::floor (n.lengthPpq * transportBlockSamplesPerBeat));
            const int64 offAt    = midiSampleClock + finalOff + durSamples;
            pianoPendingNoteOffs.add ({ pianoMidiChannel, note, offAt });
        }
        return;
    }

    const PianoHit& hit = pianoEngine.getStep (wrappedStep);
    if (! hit.active) return;

    int swingOff = pianoEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        float g = leaderMidiGain (apvtsMain);
    uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) hit.velocity * g + 0.5f));
    int   note = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (pianoMidiChannel, note, vel), finalOff);

    int durSamples = pianoEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    pianoPendingNoteOffs.add ({ pianoMidiChannel, note, offAt });
}

void BridgeProcessor::sendPianoNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = pianoPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = pianoPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            pianoPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processPianoBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncPianoEngineFromAPVTS();
    servicePianoSustainPedal (midiMessages, false);

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    pianoLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double ppqPerStep = getTransportPpqPerPatternStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;
    transportBlockPpqStart         = ppqStart;
    transportBlockSamplesPerBeat = samplesPerBeat;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getPianoPlaybackWrapBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    bool phraseBoundary = false;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * pianoTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != pianoLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != pianoLastProcessedStep)
        {
            phraseBoundary = true;

            if (pianoFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, pianoFillFromStep);
                pianoEngine.generateFill (fillStart);
                pianoFillQueued = false;
            }
            else if (pianoEngine.shouldTriggerFill())
            {
                pianoEngine.generateFill (juce::jmin (12, le0));
            }

            refreshChordsBassHintFromBass();
        }

        pianoCurrentStep.store (wrappedStep);
        pianoCurrentVisualStep.store (visualWrapped);
        pianoLastProcessedStep = globalStep;

        schedulePianoNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    servicePianoSustainPedal (midiMessages, phraseBoundary);
    sendPianoNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::scheduleGuitarNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                                int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMuteGuitar", "mainSoloGuitar"))
        return;

    if (! guitarClip.notes.empty())
    {
        const double ppqPerStep = getTransportPpqPerPatternStep();
        for (const auto& n : guitarClip.notes)
        {
            if (n.stepIndex0 != wrappedStep)
                continue;

            const double firePpq = (double) globalStep * ppqPerStep + n.microPpq;
            int finalOff = (int) std::floor ((firePpq - transportBlockPpqStart) * transportBlockSamplesPerBeat);
            finalOff = juce::jlimit (0, samplesPerBlock - 1, finalOff);

            const float g = leaderMidiGain (apvtsMain);
            const int note = juce::jlimit (0, 127, (int) n.pitch);
            const uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) n.velocity * g + 0.5f));

            midi.addEvent (juce::MidiMessage::noteOn (guitarMidiChannel, note, vel), finalOff);

            const int durSamples = juce::jmax (1, (int) std::floor (n.lengthPpq * transportBlockSamplesPerBeat));
            const int64 offAt    = midiSampleClock + finalOff + durSamples;
            guitarPendingNoteOffs.add ({ guitarMidiChannel, note, offAt });
        }
        return;
    }

    const GuitarHit& hit = guitarEngine.getStep (wrappedStep);
    if (! hit.active) return;

    int swingOff = guitarEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        float g = leaderMidiGain (apvtsMain);
    uint8 vel = (uint8) juce::jlimit (1, 127, (int) ((float) hit.velocity * g + 0.5f));
    int   note = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (guitarMidiChannel, note, vel), finalOff);

    int durSamples = guitarEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    guitarPendingNoteOffs.add ({ guitarMidiChannel, note, offAt });
}

void BridgeProcessor::sendGuitarNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = guitarPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = guitarPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            guitarPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processGuitarBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                        const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncGuitarEngineFromAPVTS();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    guitarLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double ppqPerStep = getTransportPpqPerPatternStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;
    transportBlockPpqStart         = ppqStart;
    transportBlockSamplesPerBeat = samplesPerBeat;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getGuitarPlaybackWrapBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * guitarTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != guitarLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != guitarLastProcessedStep)
        {
            if (guitarFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, guitarFillFromStep);
                guitarEngine.generateFill (fillStart);
                guitarFillQueued = false;
            }
            else if (guitarEngine.shouldTriggerFill())
            {
                guitarEngine.generateFill (juce::jmin (12, le0));
            }
        }

        guitarCurrentStep.store (wrappedStep);
        guitarCurrentVisualStep.store (visualWrapped);
        guitarLastProcessedStep = globalStep;

        scheduleGuitarNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendGuitarNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    {
        const juce::ScopedLock sl (previewLock);
        for (int i = previewNotes.size() - 1; i >= 0; --i)
        {
            auto& ev = previewNotes.getReference (i);
            if (! ev.noteOnSent)
            {
                midiMessages.addEvent (juce::MidiMessage::noteOn (ev.channel, ev.note, (juce::uint8) ev.velocity), 0);
                ev.noteOnSent = true;
            }
            ev.samplesRemaining -= numSamples;
            if (ev.samplesRemaining <= 0)
            {
                midiMessages.addEvent (juce::MidiMessage::noteOff (ev.channel, ev.note), juce::jmax (0, numSamples - 1));
                previewNotes.remove (i);
            }
        }
    }

    const auto mainEngineOn = [this] (const char* paramId) -> bool
    {
        if (auto* v = apvtsMain.getRawParameterValue (paramId))
            return v->load() > 0.5f;
        return true;
    };

    const bool drumsOn = mainEngineOn ("drumsOn");
    const bool bassOn = mainEngineOn ("bassOn");
    const bool pianoOn = mainEngineOn ("pianoOn");
    const bool guitarOn   = mainEngineOn ("guitarOn");

    if (! drumsOn)
    {
        flushPendingNoteOffs (drumsPendingNoteOffs, midiMessages);
        drumsCurrentStep.store (-1);
        drumsCurrentVisualStep.store (-1);
    }
    if (! bassOn)
    {
        flushPendingNoteOffs (bassPendingNoteOffs, midiMessages);
        bassCurrentStep.store (-1);
        bassCurrentVisualStep.store (-1);
    }
    if (! pianoOn)
    {
        flushPendingNoteOffs (pianoPendingNoteOffs, midiMessages);
        {
            const int ch = juce::jlimit (1, 16, pianoMidiChannel);
            midiMessages.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
        }
        pianoSustainLatchDown = false;
        lastPianoSustainCc64Value = 0;
        pianoCurrentStep.store (-1);
        pianoCurrentVisualStep.store (-1);
    }
    if (! guitarOn)
    {
        flushPendingNoteOffs (guitarPendingNoteOffs, midiMessages);
        guitarCurrentStep.store (-1);
        guitarCurrentVisualStep.store (-1);
    }

    auto* playhead = getPlayHead();
    juce::AudioPlayHead::CurrentPositionInfo pos {};
    const bool transportRunning = tryGetTransportPosition (playhead, pos) && pos.isPlaying;

    if (! transportRunning)
    {
        flushPendingNoteOffs (drumsPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (bassPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (pianoPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (guitarPendingNoteOffs, midiMessages);
        drumsCurrentStep.store (-1);
        drumsCurrentVisualStep.store (-1);
        bassCurrentStep.store (-1);
        bassCurrentVisualStep.store (-1);
        pianoCurrentStep.store (-1);
        pianoCurrentVisualStep.store (-1);
        guitarCurrentStep.store (-1);
        guitarCurrentVisualStep.store (-1);
        drumsJamDebtBeats = bassJamDebtBeats = pianoJamDebtBeats = guitarJamDebtBeats = 0.0;
        {
            const int ch = juce::jlimit (1, 16, pianoMidiChannel);
            midiMessages.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
        }
        pianoSustainLatchDown = false;
        lastPianoSustainCc64Value = 0;
        midiSampleClock += numSamples;
        return;
    }

    if (melodicPreviewFlushPending.exchange (false))
    {
        flushPendingNoteOffs (bassPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (pianoPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (guitarPendingNoteOffs, midiMessages);
    }

    if (drumsOn)
        processDrumsBlock (buffer, midiMessages, pos, numSamples);
    if (bassOn)
        processBassBlock (buffer, midiMessages, pos, numSamples);
    if (pianoOn)
        processPianoBlock (buffer, midiMessages, pos, numSamples);
    if (guitarOn)
        processGuitarBlock (buffer, midiMessages, pos, numSamples);

    {
        double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
        advanceJamClockAndMaybeEvolve (bpm, numSamples, drumsOn, bassOn, pianoOn, guitarOn);
    }

    midiSampleClock += numSamples;
}

void BridgeProcessor::advanceJamClockAndMaybeEvolve (double bpm, int numSamples,
                                                    bool drumsOn, bool bassOn, bool pianoOn, bool guitarOn)
{
    if (bpm < 1.0)
        bpm = 120.0;

    const double deltaBeats = ((double) numSamples / juce::jmax (1.0e-6, sampleRate)) * (bpm / 60.0);
    const int phraseSteps = readPhraseSteps (apvtsMain);
    const int phraseBars  = readPhraseBars (apvtsMain);
    juce::ignoreUnused (phraseBars);

    if (drumsOn)
    {
        double& debt = drumsJamDebtBeats;
        if (! readJamEnabled (apvtsDrums))
            debt = 0.0;
        else
        {
            const int periodBars = readJamPeriodBars (apvtsDrums);
            const double intervalBeats = (double) periodBars * 4.0;
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = phraseSteps;
                getMainSelectionBounds (phraseSteps, a, b);
                int from0 = juce::jlimit (0, phraseSteps - 1, a - 1);
                int to0   = juce::jlimit (0, phraseSteps - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncDrumsEngineFromAPVTS();
                // Jam spec: periodically "press Generate" for the current selection range.
                drumEngine.setSeed ((uint32_t) (0xA5A5A5A5u ^ (uint32_t) midiSampleClock ^ (uint32_t) from0 * 2654435761u));
                drumEngine.generatePatternRange (from0, to0, false, mlManager.get());
                drumEngine.rebuildGridPreview();
            }
        }
    }
    else
        drumsJamDebtBeats = 0.0;

    if (bassOn)
    {
        double& debt = bassJamDebtBeats;
        if (! readJamEnabled (apvtsBass))
            debt = 0.0;
        else
        {
            const int periodBars = readJamPeriodBars (apvtsBass);
            const double intervalBeats = (double) periodBars * 4.0;
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = phraseSteps;
                getMainSelectionBounds (phraseSteps, a, b);
                int from0 = juce::jlimit (0, phraseSteps - 1, a - 1);
                int to0   = juce::jlimit (0, phraseSteps - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncBassEngineFromAPVTS();
                refreshBassKickHintFromDrums();
                bassEngine.setSeed ((uint32_t) (0xBA55B00Fu ^ (uint32_t) midiSampleClock ^ (uint32_t) from0 * 2654435761u));
                bassEngine.generatePatternRange (from0, to0, false, mlManager.get());
                refreshChordsBassHintFromBass();
            }
        }
    }
    else
        bassJamDebtBeats = 0.0;

    if (pianoOn)
    {
        double& debt = pianoJamDebtBeats;
        if (! readJamEnabled (apvtsPiano))
            debt = 0.0;
        else
        {
            const int periodBars = readJamPeriodBars (apvtsPiano);
            const double intervalBeats = (double) periodBars * 4.0;
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = phraseSteps;
                getMainSelectionBounds (phraseSteps, a, b);
                int from0 = juce::jlimit (0, phraseSteps - 1, a - 1);
                int to0   = juce::jlimit (0, phraseSteps - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncPianoEngineFromAPVTS();
                refreshChordsBassHintFromBass();
                pianoEngine.setSeed ((uint32_t) (0x91A10B0Fu ^ (uint32_t) midiSampleClock ^ (uint32_t) from0 * 2654435761u));
                pianoEngine.generatePatternRange (from0, to0, false, mlManager.get());
            }
        }
    }
    else
        pianoJamDebtBeats = 0.0;

    if (guitarOn)
    {
        double& debt = guitarJamDebtBeats;
        if (! readJamEnabled (apvtsGuitar))
            debt = 0.0;
        else
        {
            const int periodBars = readJamPeriodBars (apvtsGuitar);
            const double intervalBeats = (double) periodBars * 4.0;
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = phraseSteps;
                getMainSelectionBounds (phraseSteps, a, b);
                int from0 = juce::jlimit (0, phraseSteps - 1, a - 1);
                int to0   = juce::jlimit (0, phraseSteps - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncGuitarEngineFromAPVTS();
                guitarEngine.setSeed ((uint32_t) (0x6017A2Bu ^ (uint32_t) midiSampleClock ^ (uint32_t) from0 * 2654435761u));
                guitarEngine.generatePatternRange (from0, to0, false, mlManager.get());
            }
        }
    }
    else
        guitarJamDebtBeats = 0.0;
}

void BridgeProcessor::requestAutomaticModelUpdateCheckIfDue()
{
    if (modelUpdateChecker.isUpdateCheckInProgress())
        return;

    const int now = (int) (juce::Time::getCurrentTime().toMilliseconds() / 1000);
    if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (apvtsMain.getParameter ("mlModelsLastUpdateCheckUnix")))
    {
        const int last = ip->get();
        if (last > 0 && (now - last) < (24 * 3600))
            return;
    }

    modelUpdateChecker.checkForModelUpdates ([this] (BridgeUpdateChecker::UpdateInfo info) {
        handleModelUpdateCheckResult (std::move (info));
    });
}

void BridgeProcessor::requestManualModelUpdateCheck()
{
    if (modelUpdateChecker.isUpdateCheckInProgress())
        return;

    modelUpdateChecker.checkForModelUpdates ([this] (BridgeUpdateChecker::UpdateInfo info) {
        handleModelUpdateCheckResult (std::move (info));
    });
}

void BridgeProcessor::handleModelUpdateCheckResult (BridgeUpdateChecker::UpdateInfo info)
{
    if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (apvtsMain.getParameter ("mlModelsLastUpdateCheckUnix")))
    {
        ip->beginChangeGesture();
        *ip = (int) (juce::Time::getCurrentTime().toMilliseconds() / 1000);
        ip->endChangeGesture();
    }

    if (info.isNewerThanInstalled && info.downloadUrl.isNotEmpty())
    {
        mlPendingModelUpdateVersion = info.latestModelVersion;
        mlPendingModelUpdateUrl     = info.downloadUrl;
    }
    else
    {
        mlPendingModelUpdateVersion.clear();
        mlPendingModelUpdateUrl.clear();
    }
}

void BridgeProcessor::getStateInformation (juce::MemoryBlock& data)
{
    juce::XmlElement root ("BridgeState");
    // v12: adds arrSection / sectionIntensity (main); life / velShape / hatOpen + lockKick / lockSnare / lockHats / lockPerc (drums);
    //      life / melody / followRhythm / velShape (melodic) plus instrument-specific articulation (slideAmt, voicingSpread, palmMute, strumIntensity).
    // v14: transport fixed to 16ths; jamPeriodBars 4 choices; uiTheme single Material Dark;
    //      rollSpanOctaves on melodic APVTS; jam/uiTheme migrations on load.
    // v15: phrase / jam period choices drop 16 bars; clamp legacy index 3 to 8 bars.
    // v16: clip timelines (Ableton-style note model) for drums/bass/piano/guitar.
    root.setAttribute ("bridgeVersion", 16);
    if (auto xmlM = apvtsMain.copyState().createXml())
        root.addChildElement (xmlM.release());
    if (auto xmlA = apvtsDrums.copyState().createXml())
        root.addChildElement (xmlA.release());
    if (auto xmlB = apvtsBass.copyState().createXml())
        root.addChildElement (xmlB.release());
    if (auto xmlS = apvtsPiano.copyState().createXml())
        root.addChildElement (xmlS.release());
    if (auto xmlP = apvtsGuitar.copyState().createXml())
        root.addChildElement (xmlP.release());
    root.setAttribute ("activeTab", activeTab.load());
    root.setAttribute ("mlPendingModelUpdateVersion", mlPendingModelUpdateVersion);
    root.setAttribute ("mlPendingModelUpdateUrl", mlPendingModelUpdateUrl);
    drumsClip.toXml (root, "DrumsClip");
    bassClip.toXml (root, "BassClip");
    pianoClip.toXml (root, "PianoClip");
    guitarClip.toXml (root, "GuitarClip");
    copyXmlToBinary (root, data);
}

void BridgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("BridgeState"))
        {
            const int bridgeVersion = xml->getIntAttribute ("bridgeVersion", 1);
            int tab = xml->getIntAttribute ("activeTab", 1);
            if (bridgeVersion < 2)
                tab = (tab == 0) ? 1 : 2; // legacy: 0 drums, 1 bass
            else
                tab = juce::jlimit (0, 4, tab);
            activeTab.store (tab);

            mlPendingModelUpdateVersion = xml->getStringAttribute ("mlPendingModelUpdateVersion");
            mlPendingModelUpdateUrl     = xml->getStringAttribute ("mlPendingModelUpdateUrl");

            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName (apvtsMain.state.getType()))
                {
                    juce::ValueTree pre = juce::ValueTree::fromXml (*child);
                    const juce::var legacyPocket = findNestedParamValue (pre, "pocket");
                    const juce::var legacyPhraseNorm = findNestedParamValue (pre, "phraseBars");
                    apvtsMain.replaceState (pre);
                    if (bridgeVersion < 15 && bridgeVersion >= 14)
                        migrateFourChoiceNormToThree (apvtsMain, "phraseBars", legacyPhraseNorm);
                    if (bridgeVersion < 13)
                    {
                        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("phraseBars")))
                        {
                            const int idx = c->getIndex();
                            if (idx >= 0 && idx <= 2)
                                c->setValueNotifyingHost (c->convertTo0to1 ((float) (idx + 1)));
                        }
                    }
                    if (bridgeVersion < 10 && ! legacyPocket.isVoid())
                        apvtsSetFloatFrom01 (apvtsMain, "hold", (float) static_cast<double> (legacyPocket));
                    if (bridgeVersion < 5)
                    {
                        if (apvtsMain.getRawParameterValue ("loopOn") != nullptr
                            && apvtsMain.getRawParameterValue ("loopOn")->load() > 0.5f
                            && apvtsMain.getRawParameterValue ("playbackLoopOn") != nullptr
                            && apvtsMain.getRawParameterValue ("playbackLoopOn")->load() <= 0.5f)
                        {
                            if (auto* p = apvtsMain.getParameter ("playbackLoopOn"))
                                if (auto* bp = dynamic_cast<juce::AudioParameterBool*> (p))
                                    bp->setValueNotifyingHost (1.0f);
                        }
                    }
                }
                else if (child->hasTagName (apvtsDrums.state.getType()))
                {
                    juce::ValueTree pre = juce::ValueTree::fromXml (*child);
                    const juce::var legacyPocket = findNestedParamValue (pre, "pocket");
                    const juce::var legacyPerform = findNestedParamValue (pre, "perform");
                    const juce::var legacyJamPeriodNorm = findNestedParamValue (pre, "jamPeriodBars");
                    apvtsDrums.replaceState (pre);
                    if (bridgeVersion < 15 && bridgeVersion >= 14)
                        migrateFourChoiceNormToThree (apvtsDrums, "jamPeriodBars", legacyJamPeriodNorm);
                    if (bridgeVersion < 10 && ! legacyPocket.isVoid())
                        apvtsSetFloatFrom01 (apvtsDrums, "hold", (float) static_cast<double> (legacyPocket));
                    if (bridgeVersion < 11)
                        migrateLegacyPerformToJamInterval (apvtsDrums, legacyPerform);
                }
                else if (child->hasTagName (apvtsBass.state.getType()))
                {
                    juce::ValueTree pre = juce::ValueTree::fromXml (*child);
                    const juce::var legacyPocket = findNestedParamValue (pre, "pocket");
                    const juce::var legacyPresence = findNestedParamValue (pre, "presence");
                    const juce::var legacyPerform = findNestedParamValue (pre, "perform");
                    const juce::var legacyJamPeriodNorm = findNestedParamValue (pre, "jamPeriodBars");
                    const juce::var legacyBassPhraseNorm = findNestedParamValue (pre, "phraseBars");
                    apvtsBass.replaceState (pre);
                    if (bridgeVersion < 15 && bridgeVersion >= 14)
                    {
                        migrateFourChoiceNormToThree (apvtsBass, "jamPeriodBars", legacyJamPeriodNorm);
                        migrateFourChoiceNormToThree (apvtsBass, "phraseBars", legacyBassPhraseNorm);
                    }
                    if (bridgeVersion < 10)
                    {
                        if (! legacyPocket.isVoid())
                            apvtsSetFloatFrom01 (apvtsBass, "hold", (float) static_cast<double> (legacyPocket));
                        if (! legacyPresence.isVoid())
                            apvtsSetFloatFrom01 (apvtsBass, "sustain", (float) static_cast<double> (legacyPresence));
                    }
                    if (bridgeVersion < 11)
                        migrateLegacyPerformToJamInterval (apvtsBass, legacyPerform);
                }
                else if (child->hasTagName (apvtsPiano.state.getType()))
                {
                    juce::ValueTree pre = juce::ValueTree::fromXml (*child);
                    const juce::var legacyPocket = findNestedParamValue (pre, "pocket");
                    const juce::var legacyPresence = findNestedParamValue (pre, "presence");
                    const juce::var legacyPerform = findNestedParamValue (pre, "perform");
                    const juce::var legacyJamPeriodNorm = findNestedParamValue (pre, "jamPeriodBars");
                    apvtsPiano.replaceState (pre);
                    if (bridgeVersion < 15 && bridgeVersion >= 14)
                        migrateFourChoiceNormToThree (apvtsPiano, "jamPeriodBars", legacyJamPeriodNorm);
                    if (bridgeVersion < 10)
                    {
                        if (! legacyPocket.isVoid())
                            apvtsSetFloatFrom01 (apvtsPiano, "hold", (float) static_cast<double> (legacyPocket));
                        if (! legacyPresence.isVoid())
                            apvtsSetFloatFrom01 (apvtsPiano, "sustain", (float) static_cast<double> (legacyPresence));
                    }
                    if (bridgeVersion < 11)
                        migrateLegacyPerformToJamInterval (apvtsPiano, legacyPerform);
                }
                else if (child->hasTagName (apvtsGuitar.state.getType()))
                {
                    juce::ValueTree pre = juce::ValueTree::fromXml (*child);
                    const juce::var legacyPocket = findNestedParamValue (pre, "pocket");
                    const juce::var legacyPresence = findNestedParamValue (pre, "presence");
                    const juce::var legacyPerform = findNestedParamValue (pre, "perform");
                    const juce::var legacyJamPeriodNorm = findNestedParamValue (pre, "jamPeriodBars");
                    apvtsGuitar.replaceState (pre);
                    if (bridgeVersion < 15 && bridgeVersion >= 14)
                        migrateFourChoiceNormToThree (apvtsGuitar, "jamPeriodBars", legacyJamPeriodNorm);
                    if (bridgeVersion < 10)
                    {
                        if (! legacyPocket.isVoid())
                            apvtsSetFloatFrom01 (apvtsGuitar, "hold", (float) static_cast<double> (legacyPocket));
                        if (! legacyPresence.isVoid())
                            apvtsSetFloatFrom01 (apvtsGuitar, "sustain", (float) static_cast<double> (legacyPresence));
                    }
                    if (bridgeVersion < 11)
                        migrateLegacyPerformToJamInterval (apvtsGuitar, legacyPerform);
                }
            }

            if (bridgeVersion < 14)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("uiTheme")))
                    p->setValueNotifyingHost (0.f);
                auto migJam = [] (juce::AudioProcessorValueTreeState& ap)
                {
                    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("jamPeriodBars")))
                    {
                        const int idx = c->getIndex();
                        if (idx >= 0 && idx <= 2)
                            c->setValueNotifyingHost (c->convertTo0to1 ((float) (idx + 1)));
                    }
                };
                migJam (apvtsDrums);
                migJam (apvtsBass);
                migJam (apvtsPiano);
                migJam (apvtsGuitar);
            }

            if (apvtsMain.getParameter ("mlPersonalityPresetName") != nullptr)
            {
                const auto knobs = readMainMLPersonalityKnobs10 (apvtsMain);
                const int idx = bridge::personality::indexMatchingKnobs (knobs);
                if (auto* presetPar = dynamic_cast<juce::AudioParameterChoice*> (
                        apvtsMain.getParameter ("mlPersonalityPresetName")))
                    presetPar->setValueNotifyingHost (
                        presetPar->getNormalisableRange().convertTo0to1 ((float) idx));
            }

            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("uiTheme")))
                bridge::theme::applyThemeChoiceIndex (p->getIndex());

            if (bridgeVersion >= 16)
            {
                if (drumsClip.fromXml (*xml, "DrumsClip") && ! drumsClip.notes.empty())
                    drumEngine.importFromClipTimeline (drumsClip);
                if (bassClip.fromXml (*xml, "BassClip") && ! bassClip.notes.empty())
                    bassEngine.importFromClipTimeline (bassClip);
                if (pianoClip.fromXml (*xml, "PianoClip") && ! pianoClip.notes.empty())
                    pianoEngine.importFromClipTimeline (pianoClip);
                if (guitarClip.fromXml (*xml, "GuitarClip") && ! guitarClip.notes.empty())
                    guitarEngine.importFromClipTimeline (guitarClip);
            }

            refreshClipTimelinesFromEngines();
        }
    }
}

#if BRIDGE_ENABLE_QA_HOOKS
void BridgeProcessor::bridgeQaInjectBassPendingNoteOff (int channel, int noteNumber, int64 atSample)
{
    bassPendingNoteOffs.add ({ juce::jlimit (1, 16, channel),
                                juce::jlimit (0, 127, noteNumber),
                                atSample });
}

bool BridgeProcessor::bridgeQaMelodicFlushPendingForTests() const noexcept
{
    return melodicPreviewFlushPending.load();
}
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BridgeProcessor();
}
