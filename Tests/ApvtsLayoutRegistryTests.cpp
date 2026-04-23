#include <JuceHeader.h>
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
    }
};

static ApvtsLayoutRegistryTests apvtsLayoutRegistryTests;
