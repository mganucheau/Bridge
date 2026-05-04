#include <JuceHeader.h>
#include "drums/DrumEngine.h"
#include "drums/DrumPatternMetrics.h"
#include "drums/DrumsStylePresets.h"

namespace
{
int countActive (const DrumEngine& e, int r0, int r1)
{
    r0 = juce::jlimit (0, NUM_STEPS - 1, r0);
    r1 = juce::jlimit (0, NUM_STEPS - 1, r1);
    if (r1 < r0)
        std::swap (r0, r1);
    int c = 0;
    for (int s = r0; s <= r1; ++s)
        for (int d = 0; d < NUM_DRUMS; ++d)
            if (e.getStep (s)[(size_t) d].active)
                ++c;
    return c;
}
} // namespace

struct DrumHumanFeelMetricsTests final : public juce::UnitTest
{
    DrumHumanFeelMetricsTests()
        : juce::UnitTest ("Drum human feel: density caps + polyphony invariants", "Bridge/QA")
    {
    }

    void runTest() override
    {
        {
            DrumEngine e;
            e.setPatternLen (NUM_STEPS);
            e.setStyle (0);
            e.setSeed (77);
            e.setDensity (1.0f);
            e.setComplexity (1.0f);
            e.generatePattern (false, nullptr);

            const auto st = bridge::drums::measurePattern (e.getPattern(), e.getPatternLen());
            // Must stay below ~55% of cells even at max density (human-feel cap).
            expect (st.activeRate < 0.55, "full generate at max density should not saturate the grid");
            expect (st.maxStepPolyphony <= 6, "per-step voice cap");
        }

        {
            DrumEngine e;
            e.setPatternLen (NUM_STEPS);
            e.setStyle (9); // DnB
            e.setSeed (42);
            e.setDensity (0.0f);
            e.morphPatternForDensityAndComplexity (0, NUM_STEPS - 1);
            e.setDensity (1.0f);
            e.morphPatternForDensityAndComplexity (0, NUM_STEPS - 1);
            const auto st = bridge::drums::measurePattern (e.getPattern(), e.getPatternLen());
            expect (st.activeRate < 0.55, "morph at d=1 should not fill the grid");
            expect (st.maxStepPolyphony <= 6, "morph polyphony cap");
        }

        {
            DrumEngine e;
            e.setPatternLen (NUM_STEPS);
            e.setStyle (0);
            e.setDensity (0.0f);
            e.morphPatternForDensityAndComplexity (3, 5);
            e.setDensity (1.0f);
            e.morphPatternForDensityAndComplexity (3, 5);
            const int  slots  = 3 * NUM_DRUMS;
            const int  active = countActive (e, 3, 5);
            const double rate  = (double) active / (double) juce::jmax (1, slots);
            expect (rate < 0.55, "partial range morph at max d stays below cap");
        }

        beginTest ("Manual listening QA (export MIDI; not automated)");
        logMessage (juce::String (
            "Compare to Sting/Session reference: downbeats, hat texture (not a solid wall), ghosts, pocket. "
            "CI enforces activeRate < 55% and max 6 voices/step at max knobs."));
        expect (true);
    }
};

static DrumHumanFeelMetricsTests drumHumanFeelMetricsTests;
