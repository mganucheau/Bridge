#include <JuceHeader.h>
#include <cmath>
#include "BridgeProcessor.h"

namespace
{
void expectHasParam (juce::UnitTest& t, juce::AudioProcessorValueTreeState& ap, const char* id)
{
    t.expect (ap.getParameter (id) != nullptr, juce::String ("Missing parameter: ") + id);
}

void expectNoParam (juce::UnitTest& t, juce::AudioProcessorValueTreeState& ap, const char* id)
{
    t.expect (ap.getParameter (id) == nullptr, juce::String ("Unexpected parameter: ") + id);
}
} // namespace

struct ApvtsLayoutRegistryTests final : public juce::UnitTest
{
    ApvtsLayoutRegistryTests()
        : juce::UnitTest ("APVTS layout registry (prompt / UI contract)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        BridgeProcessor proc;

        beginTest ("Main APVTS exposes leader, engines, hold, tonality, transport — no legacy pocket/presence/sustain");

        static constexpr const char* mainIds[] = {
            "leaderTabOn", "drumsOn", "bassOn", "pianoOn", "guitarOn",
            "mainMuteDrums", "mainSoloDrums", "mainMuteBass", "mainSoloBass",
            "mainMutePiano", "mainSoloPiano", "mainMuteGuitar", "mainSoloGuitar",
            "leaderStyle", "density", "swing", "humanize", "hold", "velocity",
            "fillRate", "complexity", "ghostAmount", "loopStart", "loopEnd",
            "loopOn", "playbackLoopOn", "loopSpanLock", "tickerSpeed",
            "scale", "rootNote", "octave", "hostSync", "transportPlaying", "internalBpm",
            "timeDivision", "uiTheme",
            "mlPersRhythmTight", "mlPersDynamicRange", "mlPersTimbreTexture", "mlPersTensionArc",
            "mlPersTempoVolatility", "mlPersEmotionalTemp", "mlPersHarmAdventure", "mlPersStructPredict",
            "mlPersShowmanship", "mlPersGenreLoyalty", "mlPersonalityPresetName", "mlModelsLastUpdateCheckUnix",
        };
        for (auto* id : mainIds)
            expectHasParam (*this, proc.apvtsMain, id);

        expectNoParam (*this, proc.apvtsMain, "perform");
        expectNoParam (*this, proc.apvtsMain, "pocket");
        expectNoParam (*this, proc.apvtsMain, "presence");
        expectNoParam (*this, proc.apvtsMain, "sustain");

        beginTest ("Drums APVTS: hold, no sustain / pocket");

        static constexpr const char* drumsIds[] = {
            "style", "density", "swing", "humanize", "velocity", "fillRate", "complexity",
            "hold", "ghostAmount", "intensity", "midiChannel", "loopStart", "loopEnd",
            "loopOn", "jamInterval", "tickerSpeed",
        };
        for (auto* id : drumsIds)
            expectHasParam (*this, proc.apvtsDrums, id);
        expectNoParam (*this, proc.apvtsDrums, "perform");
        expectNoParam (*this, proc.apvtsDrums, "pocket");
        expectNoParam (*this, proc.apvtsDrums, "presence");
        expectNoParam (*this, proc.apvtsDrums, "sustain");

        beginTest ("Melodic APVTS (bass / piano / guitar): hold + sustain, no pocket");

        static constexpr const char* melodicIds[] = {
            "style", "jamInterval", "temperature", "density", "swing", "humanize", "hold",
            "velocity", "fillRate", "complexity", "ghostAmount", "staccato", "sustain",
            "intensity", "patternLen", "locked", "midiChannel", "phraseBars",
            "loopStart", "loopEnd", "loopOn", "tickerSpeed",
        };
        for (auto* id : melodicIds)
        {
            expectHasParam (*this, proc.apvtsBass, id);
            expectHasParam (*this, proc.apvtsPiano, id);
            expectHasParam (*this, proc.apvtsGuitar, id);
        }
        expectNoParam (*this, proc.apvtsBass, "perform");
        expectNoParam (*this, proc.apvtsPiano, "perform");
        expectNoParam (*this, proc.apvtsGuitar, "perform");
        expectNoParam (*this, proc.apvtsBass, "pocket");
        expectNoParam (*this, proc.apvtsPiano, "pocket");
        expectNoParam (*this, proc.apvtsGuitar, "pocket");
        expectNoParam (*this, proc.apvtsBass, "presence");
        expectNoParam (*this, proc.apvtsPiano, "presence");
        expectNoParam (*this, proc.apvtsGuitar, "presence");

        beginTest ("Fresh processor: groove-style floats default to 0 (drums velocity modest default), density and complexity to 0.5");

        auto near = [this] (float a, float b, const char* label)
        {
            expect (std::abs (a - b) < 1.0e-4f, juce::String (label) + " default mismatch");
        };

        near ((float) *proc.apvtsMain.getRawParameterValue ("humanize"), 0.f, "main humanize");
        near ((float) *proc.apvtsMain.getRawParameterValue ("hold"), 0.f, "main hold");
        near ((float) *proc.apvtsMain.getRawParameterValue ("velocity"), 0.f, "main velocity");
        near ((float) *proc.apvtsMain.getRawParameterValue ("ghostAmount"), 0.f, "main ghostAmount");
        near ((float) *proc.apvtsMain.getRawParameterValue ("fillRate"), 0.f, "main fillRate");
        near ((float) *proc.apvtsMain.getRawParameterValue ("density"), 0.5f, "main density");
        near ((float) *proc.apvtsMain.getRawParameterValue ("complexity"), 0.5f, "main complexity");

        near ((float) *proc.apvtsDrums.getRawParameterValue ("humanize"), 0.f, "drums humanize");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("velocity"), 0.72f, "drums velocity");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("fillRate"), 0.f, "drums fillRate");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("hold"), 0.f, "drums hold");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("ghostAmount"), 0.f, "drums ghostAmount");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("intensity"), 0.f, "drums intensity");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("density"), 0.5f, "drums density");
        near ((float) *proc.apvtsDrums.getRawParameterValue ("complexity"), 0.5f, "drums complexity");

        near ((float) *proc.apvtsBass.getRawParameterValue ("humanize"), 0.f, "bass humanize");
        near ((float) *proc.apvtsBass.getRawParameterValue ("velocity"), 0.f, "bass velocity");
        near ((float) *proc.apvtsBass.getRawParameterValue ("fillRate"), 0.f, "bass fillRate");
        near ((float) *proc.apvtsBass.getRawParameterValue ("hold"), 0.f, "bass hold");
        near ((float) *proc.apvtsBass.getRawParameterValue ("ghostAmount"), 0.f, "bass ghostAmount");
        near ((float) *proc.apvtsBass.getRawParameterValue ("staccato"), 0.f, "bass staccato");
        near ((float) *proc.apvtsBass.getRawParameterValue ("intensity"), 0.f, "bass intensity");
        near ((float) *proc.apvtsBass.getRawParameterValue ("density"), 0.5f, "bass density");
        near ((float) *proc.apvtsBass.getRawParameterValue ("complexity"), 0.5f, "bass complexity");
    }
};

static ApvtsLayoutRegistryTests apvtsLayoutRegistryTests;
