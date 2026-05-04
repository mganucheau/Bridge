#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgeQaTestHelpers.h"
#include "bass/BassStylePresets.h"

namespace
{
int expectedRootMidiBase (int rootNote0to11, int octave1to4)
{
    return rootNote0to11 + (octave1to4 + 1) * 12;
}
} // namespace

struct TonalityEngineSyncTests final : public juce::UnitTest
{
    TonalityEngineSyncTests()
        : juce::UnitTest ("Tonality: main APVTS syncs melodic engines", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("rootNote + octave update bass / piano / guitar root bases");

        BridgeProcessor proc;
        bridgeQaSetIntParam (proc.apvtsMain, "rootNote", 7);
        bridgeQaSetIntParam (proc.apvtsMain, "octave", 2);

        const int want = expectedRootMidiBase (7, 2);
        expectEquals (proc.bassEngine.getRootMidiBase(), want);
        expectEquals (proc.pianoEngine.getRootMidiBase(), want);
        expectEquals (proc.guitarEngine.getRootMidiBase(), want);

        beginTest ("default main scale is Major (index 2)");

        if (proc.apvtsMain.getRawParameterValue ("scale") != nullptr)
            expectEquals ((int) proc.apvtsMain.getRawParameterValue ("scale")->load(), 2);
        else
            expect (false, "Main APVTS missing scale parameter");

        beginTest ("scale index propagates to bass engine");

        if (auto* sp = dynamic_cast<juce::AudioParameterInt*> (proc.apvtsMain.getParameter ("scale")))
        {
            const int target = juce::jmin (BassPreset::NUM_SCALES - 1, 3);
            sp->setValueNotifyingHost (sp->getNormalisableRange().convertTo0to1 ((float) target));
            expectEquals (proc.bassEngine.getScale(), target);
            expectEquals (proc.pianoEngine.getScale(), target);
            expectEquals (proc.guitarEngine.getScale(), target);
        }
        else
        {
            expect (false, "Main APVTS missing int parameter scale");
        }
    }
};

static TonalityEngineSyncTests tonalityEngineSyncTests;
