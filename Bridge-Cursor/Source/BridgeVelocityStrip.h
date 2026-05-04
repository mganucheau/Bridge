#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

/** Slim per-step velocity bar graph. Owns no data — pulls velocities (0..127) for each step
    via a supplier callback. Used under each instrument grid so users can see the velocity
    contour shaped by humanize, the velShape macro, and the Life drift. */
class BridgeVelocityStrip : public juce::Component,
                            private juce::Timer
{
public:
    BridgeVelocityStrip (int numSteps, juce::Colour accent);

    /** Returns step velocity in 0..127 for the given step index, or 0 if inactive. */
    std::function<int (int /*step*/)> velocityAt;

    void paint (juce::Graphics& g) override;
    void setStepRange (int from0, int to0); // optional dimming outside the loop range

    /** Update how many steps the strip visualises. */
    void setNumSteps (int steps);
    int  getNumSteps() const noexcept { return numSteps; }

    /** PatternFlow-style tiling: visually repeat the per-step velocity bars across N bars. The
        callback `velocityAt` continues to be sampled for `numSteps` cells (one bar) and the strip
        tiles them — so the bar contour repeats and stays aligned with the grid. */
    void setBarRepeats (int repeats);
    int  getBarRepeats() const noexcept { return barRepeats; }

private:
    void timerCallback() override;

    int                 numSteps;
    juce::Colour        accent;
    int                 rangeFrom = 0;
    int                 rangeTo   = -1;
    int                 barRepeats = 1;
    std::array<float, 256> snapshot {};
};
