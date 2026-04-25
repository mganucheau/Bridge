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
#include <array>
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

static int readJamIntervalChoiceIndex (juce::AudioProcessorValueTreeState& ap)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("jamInterval")))
        return c->getIndex();
    return 0;
}

/** jamInterval index 0 = Off; 1..7 → quarter-note beats. */
static double jamIntervalBeatsFromChoiceIndex (int idx)
{
    static constexpr double table[8] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    idx = juce::jlimit (0, 7, idx);
    return table[(size_t) idx];
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
    layout.add (std::make_unique<juce::AudioParameterBool> ("leaderTabOn", "Leader tab", true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("drumsOn",  "Drums on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("bassOn",  "Bass on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("pianoOn",  "Keys on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("guitarOn",    "Guitar on",    true));

    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteDrums", "Mute Drums", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloDrums", "Solo Drums", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteBass", "Mute Bass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloBass", "Solo Bass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMutePiano", "Mute Keys", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloPiano", "Solo Keys", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainMuteGuitar",   "Mute Guitar",   false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("mainSoloGuitar",   "Solo Guitar",   false));

    juce::StringArray leaderStyleNames;
    for (int i = 0; i < NUM_LEADER_STYLES; ++i)
        leaderStyleNames.add (LEADER_STYLE_NAMES[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> ("leaderStyle", "Arrangement", leaderStyleNames, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("density", "Density", 0.f, 1.f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing", "Swing", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize", "Humanize", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hold", "Hold", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity", "Velocity", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate", "Fill Rate", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity", "Complexity", 0.f, 1.f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost", 0.f, 1.f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart", "Loop Start", 1, 16, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd", "Loop End", 1, 16, 16));
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

    juce::StringArray themeNames;
    const char* pairs[] = {
        "Material Light", "Material Dark",
        "Apple HIG Light", "Apple HIG Dark",
        "Fluent Light", "Fluent Dark",
        "Carbon Light", "Carbon Dark",
        "Spectrum Light", "Spectrum Dark",
        "Atlassian Light", "Atlassian Dark",
        "Ant Light", "Ant Dark",
        "Radix Sand", "Radix Slate",
        "Nord Snow", "Nord Polar Night",
        "Tokyo Day", "Tokyo Night",
        "Rose Pine Dawn", "Rose Pine Moon"
    };
    for (auto* n : pairs)
        themeNames.add (n);
    layout.add (std::make_unique<juce::AudioParameterChoice> ("uiTheme", "UI theme", themeNames, 1));

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
    // Swing, humanize, fill, hold at 0 (neutral). Drums velocity starts above 0 so GM hits stay
    // audible after leader gain; melodic lanes keep velocity at 0 for neutral expression.

    juce::StringArray styleNames;
    for (int i = 0; i < bridgeUnifiedStyleCount(); ++i)
        styleNames.add (bridgeUnifiedStyleNames()[i]);

    layout.add (std::make_unique<juce::AudioParameterChoice> ("style", "Style", styleNames, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.72f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.5f));
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
    layout.add (std::make_unique<juce::AudioParameterFloat> ("temperature", "Temperature", 0.01f, 2.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("hold",        "Hold",        0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("staccato",    "Staccato",    0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("sustain",     "Sustain",     0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("intensity",   "Intensity",   0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("patternLen",  "Pattern Len", 4,     16,   16));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("locked",      "Locked",      false));
    layout.add (std::make_unique<juce::AudioParameterInt> ("midiChannel",  "MIDI Channel", 1, 16, defaultMidiChannel));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("phraseBars", "Bar Grid",
                                                              juce::StringArray { "4 bars", "8 bars", "16 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart",   "Loop Start",   1, BassPreset::NUM_STEPS, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd",     "Loop End",     1, BassPreset::NUM_STEPS, BassPreset::NUM_STEPS));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopOn", "Loop On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("tickerSpeed", "Speed",
                                                              juce::StringArray { "x2", "1", "1/2" }, 1));

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
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyDrumsPatternChanged();
    };

    bassEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyBassPatternChanged();
    };

    pianoEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyPianoPatternChanged();
    };

    guitarEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyGuitarPatternChanged();
    };

    apvtsMain.addParameterListener ("loopStart", this);
    apvtsMain.addParameterListener ("loopEnd", this);
    apvtsMain.addParameterListener ("loopSpanLock", this);
    apvtsMain.addParameterListener ("scale", this);
    apvtsMain.addParameterListener ("rootNote", this);
    apvtsMain.addParameterListener ("octave", this);
    apvtsMain.addParameterListener ("uiTheme", this);

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
    apvtsMain.removeParameterListener ("uiTheme", this);
}

juce::AudioProcessorEditor* BridgeProcessor::createEditor()
{
    return new BridgeEditor (*this);
}

void BridgeProcessor::refreshBassKickHintFromDrums()
{
    std::vector<float> hint (16, 0.0f);
    const auto& pat = drumEngine.getPattern();
    for (int s = 0; s < 16; ++s)
        hint[(size_t) s] = pat[(size_t) s][0].active ? 1.0f : 0.0f;
    bassEngine.setBassMLKickHint (hint);
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
    drumEngine.generatePattern (false, mlManager.get());
    drumEngine.setPlaybackSamplesPerStep (sampleRate * 60.0 / 120.0 / 4.0);
    syncDrumsEngineFromAPVTS();
    drumEngine.rebuildGridPreview();

    bassLastProcessedStep = -1;
    bassPendingNoteOffs.clear();
    syncBassEngineFromAPVTS();
    refreshBassKickHintFromDrums();
    bassEngine.generatePattern (false, mlManager.get());
    bassEngine.rebuildGridPreview();
    refreshChordsBassHintFromBass();

    pianoLastProcessedStep = -1;
    pianoPendingNoteOffs.clear();
    syncPianoEngineFromAPVTS();
    pianoEngine.generatePattern (false, mlManager.get());
    pianoEngine.rebuildGridPreview();

    guitarLastProcessedStep = -1;
    guitarPendingNoteOffs.clear();
    syncGuitarEngineFromAPVTS();
    guitarEngine.generatePattern (false, mlManager.get());
    guitarEngine.rebuildGridPreview();
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
    if (playbackLoopEngaged())
        getMainSelectionBounds (NUM_STEPS, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = NUM_STEPS;
    }
}

void BridgeProcessor::getBassPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    if (playbackLoopEngaged())
        getMainSelectionBounds (BassPreset::NUM_STEPS, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = BassPreset::NUM_STEPS;
    }
}

void BridgeProcessor::getPianoPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    if (playbackLoopEngaged())
        getMainSelectionBounds (PianoPreset::NUM_STEPS, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = PianoPreset::NUM_STEPS;
    }
}

void BridgeProcessor::getGuitarPlaybackWrapBounds (int& loopStart, int& loopEnd) const
{
    if (playbackLoopEngaged())
        getMainSelectionBounds (GuitarPreset::NUM_STEPS, loopStart, loopEnd);
    else
    {
        loopStart = 1;
        loopEnd = GuitarPreset::NUM_STEPS;
    }
}

void BridgeProcessor::getDrumsLoopBounds (int& loopStart, int& loopEnd) const
{
    getMainSelectionBounds (NUM_STEPS, loopStart, loopEnd);
}

void BridgeProcessor::getBassLoopBounds (int& loopStart, int& loopEnd) const
{
    getMainSelectionBounds (BassPreset::NUM_STEPS, loopStart, loopEnd);
}

double BridgeProcessor::getMainPpqPerStep() const noexcept
{
    int idx = 3;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("timeDivision")))
        idx = p->getIndex();
    else if (auto* v = apvtsMain.getRawParameterValue ("timeDivision"))
        idx = (int) v->load();

    idx = juce::jlimit (0, 9, idx);
    static constexpr double table[10] = {
        0.0625,              // 1/64
        0.125,               // 1/32
        1.0 / 6.0,           // 1/24 T
        0.25,                // 1/16
        1.0 / 3.0,           // 1/12 T
        0.5,                 // 1/8
        2.0 / 3.0,           // 1/6 T
        1.0,                 // 1/4
        2.0,                 // 1/2
        4.0                  // 1 bar
    };
    return table[(size_t) idx];
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

    if (parameterID == "uiTheme")
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvtsMain.getParameter ("uiTheme")))
            bridge::theme::applyThemeChoiceIndex (p->getIndex());
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->repaint();
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
    humanize *= (1.0f - 0.55f * L.hold * 0.45f);
    swing    *= (1.0f - 0.30f * L.hold * 0.45f);
    hold      = juce::jmin (1.f, hold + 0.38f * L.hold);
    density  *= (1.0f - 0.48f * L.swing);
    fillRate  = juce::jmin (1.f, fillRate + 0.40f * L.complexity);
    ghostAmt  = juce::jmin (1.f, ghostAmt + 0.32f * L.complexity);
    complexity = juce::jmin (1.f, complexity + 0.22f * L.complexity);
    velocity  = juce::jmin (1.f, velocity * (0.90f + 0.14f * L.humanize));

    drumEngine.setDensity    (density);
    drumEngine.setSwing      (swing);
    drumEngine.setHumanize   (humanize);
    drumEngine.setVelocity   (velocity);
    drumEngine.setFillRate   (fillRate);
    drumEngine.setComplexity (complexity);
    drumEngine.setHold       (hold);
    drumEngine.setGhostAmount (ghostAmt);
    drumEngine.setPatternLen (NUM_STEPS);

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsDrums.getParameter ("tickerSpeed")))
        drumsTickerRate = drumsTickerRateForChoiceIndex (tr->getIndex());
    else
        drumsTickerRate = drumsTickerRateForChoiceIndex ((int) *apvtsDrums.getRawParameterValue ("tickerSpeed"));

    drumEngine.setPhraseBars (4);

    drumsMidiChannel = juce::jlimit (1, 16, (int) *apvtsDrums.getRawParameterValue ("midiChannel"));

    drumsAnySolo = false;
    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        drumsLaneMute[(size_t) drum] = ((bool) *apvtsDrums.getRawParameterValue ("mute_" + juce::String (drum)));
        drumsLaneSolo[(size_t) drum] = ((bool) *apvtsDrums.getRawParameterValue ("solo_" + juce::String (drum)));
        drumsAnySolo = drumsAnySolo || drumsLaneSolo[(size_t) drum];
    }

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

void BridgeProcessor::triggerDrumsGenerate()
{
    juce::Random r;
    drumEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncDrumsEngineFromAPVTS();
    if (isMainSelectionFullClip (NUM_STEPS))
        drumEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = NUM_STEPS;
        getMainSelectionBounds (NUM_STEPS, a, b);
        drumEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
}

void BridgeProcessor::triggerDrumsFill (int fromStep)
{
    int a = 1, b = NUM_STEPS;
    getMainSelectionBounds (NUM_STEPS, a, b);
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
    bassEngine.setPatternLen  (BassPreset::NUM_STEPS);
    bassEngine.setLocked      ((bool) *apvtsBass.getRawParameterValue ("locked"));

    bassMidiChannel = juce::jlimit (1, 16, (int)*apvtsBass.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsBass.getRawParameterValue ("phraseBars");
    bassEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsBass.getParameter ("tickerSpeed")))
        bassTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        bassTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsBass.getRawParameterValue ("tickerSpeed"));

    syncBassMLPersonalityToEngine (apvtsMain, bassEngine);
}

void BridgeProcessor::triggerBassGenerate()
{
    juce::Random r;
    bassEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncBassEngineFromAPVTS();
    refreshBassKickHintFromDrums();
    if (isMainSelectionFullClip (BassPreset::NUM_STEPS))
        bassEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = BassPreset::NUM_STEPS;
        getMainSelectionBounds (BassPreset::NUM_STEPS, a, b);
        bassEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
    refreshChordsBassHintFromBass();
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
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyBassPatternChanged();
}

void BridgeProcessor::triggerBassFill (int fromStep)
{
    int a = 1, b = BassPreset::NUM_STEPS;
    getMainSelectionBounds (BassPreset::NUM_STEPS, a, b);
    bassFillQueued   = true;
    bassFillFromStep = juce::jlimit (a, b, fromStep);
}

void BridgeProcessor::getPianoLoopBounds (int& loopStart, int& loopEnd) const
{
    getMainSelectionBounds (PianoPreset::NUM_STEPS, loopStart, loopEnd);
}

void BridgeProcessor::getGuitarLoopBounds (int& loopStart, int& loopEnd) const
{
    getMainSelectionBounds (GuitarPreset::NUM_STEPS, loopStart, loopEnd);
}

void BridgeProcessor::morphAllEnginesToMainSelection()
{
    syncDrumsEngineFromAPVTS();
    syncBassEngineFromAPVTS();
    syncPianoEngineFromAPVTS();
    syncGuitarEngineFromAPVTS();

    int ls = 1, le = 1;
    getMainSelectionBounds (NUM_STEPS, ls, le);
    drumEngine.morphPatternForDensityAndComplexity (
        ls - 1, juce::jmin (le - 1, drumEngine.getPatternLen() - 1));

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
    pianoEngine.setPatternLen  (PianoPreset::NUM_STEPS);
    pianoEngine.setLocked      ((bool) *apvtsPiano.getRawParameterValue ("locked"));

    pianoMidiChannel = juce::jlimit (1, 16, (int)*apvtsPiano.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsPiano.getRawParameterValue ("phraseBars");
    pianoEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsPiano.getParameter ("tickerSpeed")))
        pianoTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        pianoTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsPiano.getRawParameterValue ("tickerSpeed"));

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
    guitarEngine.setPatternLen  (GuitarPreset::NUM_STEPS);
    guitarEngine.setLocked      ((bool) *apvtsGuitar.getRawParameterValue ("locked"));

    guitarMidiChannel = juce::jlimit (1, 16, (int)*apvtsGuitar.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsGuitar.getRawParameterValue ("phraseBars");
    guitarEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsGuitar.getParameter ("tickerSpeed")))
        guitarTickerRate = bassTickerRateForChoiceIndex (tr->getIndex());
    else
        guitarTickerRate = bassTickerRateForChoiceIndex ((int) *apvtsGuitar.getRawParameterValue ("tickerSpeed"));

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
    if (isMainSelectionFullClip (PianoPreset::NUM_STEPS))
        pianoEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = PianoPreset::NUM_STEPS;
        getMainSelectionBounds (PianoPreset::NUM_STEPS, a, b);
        pianoEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
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
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyPianoPatternChanged();
}

void BridgeProcessor::triggerPianoFill (int fromStep)
{
    int a = 1, b = PianoPreset::NUM_STEPS;
    getMainSelectionBounds (PianoPreset::NUM_STEPS, a, b);
    pianoFillQueued   = true;
    pianoFillFromStep = juce::jlimit (a, b, fromStep);
}

void BridgeProcessor::triggerGuitarGenerate()
{
    juce::Random r;
    guitarEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
    syncGuitarEngineFromAPVTS();
    if (isMainSelectionFullClip (GuitarPreset::NUM_STEPS))
        guitarEngine.generatePattern (false, mlManager.get());
    else
    {
        int a = 1, b = GuitarPreset::NUM_STEPS;
        getMainSelectionBounds (GuitarPreset::NUM_STEPS, a, b);
        guitarEngine.generatePatternRange (a - 1, b - 1, false, mlManager.get());
    }
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

    DrumStep hits;
    if (drumEngine.isFillHoldActive())
        drumEngine.evaluateStepForPlayback (globalStep, wrappedStep, hits, samplesPerStep);
    else
        hits = drumEngine.getStep (wrappedStep);

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        if (! isDrumsLaneAudible (drum)) continue;
        if (! hits[(size_t) drum].active) continue;

        int swingOff = drumEngine.getSwingOffset (wrappedStep, samplesPerStep);
        int humanOff = hits[(size_t) drum].timeShift;
        int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        // GM drum map (see DRUM_MIDI_NOTES); default MIDI channel 10 — drum racks must listen on
        // that channel (or omni). Melodic instruments on ch 1 only will play bass, not drums.
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
    const double ppqPerStep = getMainPpqPerStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;
    drumEngine.setPlaybackSamplesPerStep (samplesPerStep);

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

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
            else if (drumEngine.shouldTriggerFill())
            {
                drumEngine.generateFill (juce::jmin (12, le0));
            }
        }

        drumsCurrentStep.store (wrappedStep);
        drumsCurrentVisualStep.store (visualWrapped);
        drumsLastProcessedStep = globalStep;

        scheduleDrumsNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendDrumsNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::scheduleBassNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                   int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMuteBass", "mainSoloBass"))
        return;

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
    const double ppqPerStep = getMainPpqPerStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

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

void BridgeProcessor::schedulePianoNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                  int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMutePiano", "mainSoloPiano"))
        return;

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
    const double ppqPerStep = getMainPpqPerStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

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

void BridgeProcessor::scheduleGuitarNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                int sampleOffset, juce::MidiBuffer& midi)
{
    if (! mainRowMidiOpen (apvtsMain, "mainMuteGuitar", "mainSoloGuitar"))
        return;

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
    const double ppqPerStep = getMainPpqPerStep();
    double samplesPerStep = samplesPerBeat * ppqPerStep;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

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

    if (drumsOn)
    {
        double& debt = drumsJamDebtBeats;
        const int ji = readJamIntervalChoiceIndex (apvtsDrums);
        const double intervalBeats = jamIntervalBeatsFromChoiceIndex (ji);
        if (intervalBeats <= 0.0)
            debt = 0.0;
        else
        {
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = NUM_STEPS;
                getMainSelectionBounds (NUM_STEPS, a, b);
                int from0 = juce::jlimit (0, NUM_STEPS - 1, a - 1);
                int to0   = juce::jlimit (0, NUM_STEPS - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncDrumsEngineFromAPVTS();
                drumEngine.evolvePatternRangeForJam (from0, to0, mlManager.get());
                drumEngine.rebuildGridPreview();
            }
        }
    }
    else
        drumsJamDebtBeats = 0.0;

    if (bassOn)
    {
        double& debt = bassJamDebtBeats;
        const int ji = readJamIntervalChoiceIndex (apvtsBass);
        const double intervalBeats = jamIntervalBeatsFromChoiceIndex (ji);
        if (intervalBeats <= 0.0)
            debt = 0.0;
        else
        {
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = BassPreset::NUM_STEPS;
                getMainSelectionBounds (BassPreset::NUM_STEPS, a, b);
                int from0 = juce::jlimit (0, BassPreset::NUM_STEPS - 1, a - 1);
                int to0   = juce::jlimit (0, BassPreset::NUM_STEPS - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncBassEngineFromAPVTS();
                refreshBassKickHintFromDrums();
                bassEngine.evolvePatternRangeForJam (from0, to0, mlManager.get());
                refreshChordsBassHintFromBass();
            }
        }
    }
    else
        bassJamDebtBeats = 0.0;

    if (pianoOn)
    {
        double& debt = pianoJamDebtBeats;
        const int ji = readJamIntervalChoiceIndex (apvtsPiano);
        const double intervalBeats = jamIntervalBeatsFromChoiceIndex (ji);
        if (intervalBeats <= 0.0)
            debt = 0.0;
        else
        {
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = PianoPreset::NUM_STEPS;
                getMainSelectionBounds (PianoPreset::NUM_STEPS, a, b);
                int from0 = juce::jlimit (0, PianoPreset::NUM_STEPS - 1, a - 1);
                int to0   = juce::jlimit (0, PianoPreset::NUM_STEPS - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncPianoEngineFromAPVTS();
                refreshChordsBassHintFromBass();
                pianoEngine.evolvePatternRangeForJam (from0, to0, mlManager.get());
            }
        }
    }
    else
        pianoJamDebtBeats = 0.0;

    if (guitarOn)
    {
        double& debt = guitarJamDebtBeats;
        const int ji = readJamIntervalChoiceIndex (apvtsGuitar);
        const double intervalBeats = jamIntervalBeatsFromChoiceIndex (ji);
        if (intervalBeats <= 0.0)
            debt = 0.0;
        else
        {
            debt += deltaBeats;
            while (debt >= intervalBeats)
            {
                debt -= intervalBeats;
                int a = 1, b = GuitarPreset::NUM_STEPS;
                getMainSelectionBounds (GuitarPreset::NUM_STEPS, a, b);
                int from0 = juce::jlimit (0, GuitarPreset::NUM_STEPS - 1, a - 1);
                int to0   = juce::jlimit (0, GuitarPreset::NUM_STEPS - 1, b - 1);
                if (to0 < from0)
                    std::swap (from0, to0);
                syncGuitarEngineFromAPVTS();
                guitarEngine.evolvePatternRangeForJam (from0, to0, mlManager.get());
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
    root.setAttribute ("bridgeVersion", 11);
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
    copyXmlToBinary (root, data);
}

void BridgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("BridgeState"))
        {
            const int bridgeVersion = xml->getIntAttribute ("bridgeVersion", 1);
            int tab = xml->getIntAttribute ("activeTab", 0);
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
                    apvtsMain.replaceState (pre);
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
                    apvtsDrums.replaceState (pre);
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
                    apvtsBass.replaceState (pre);
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
                    apvtsPiano.replaceState (pre);
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
                    apvtsGuitar.replaceState (pre);
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
