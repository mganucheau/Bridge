#pragma once

#include <JuceHeader.h>
#include "BridgePhrase.h"

namespace bridge
{

/** Binds a Slider to main APVTS `loopStart` / `loopEnd` with range 1…phraseSteps (phraseBars). */
struct MainLoopKnobSliderAttachment final : private juce::Slider::Listener,
                                            private juce::AudioProcessorValueTreeState::Listener
{
    MainLoopKnobSliderAttachment (juce::AudioProcessorValueTreeState& mainApvts,
                                  juce::Slider& sliderIn,
                                  const juce::String& paramIdIn)
        : ap (mainApvts), s (sliderIn), pid (paramIdIn)
    {
        s.addListener (this);
        ap.addParameterListener ("phraseBars", this);
        ap.addParameterListener ("loopStart", this);
        ap.addParameterListener ("loopEnd", this);
        refreshRangeAndValue();
    }

    ~MainLoopKnobSliderAttachment() override
    {
        ap.removeParameterListener ("phraseBars", this);
        ap.removeParameterListener ("loopStart", this);
        ap.removeParameterListener ("loopEnd", this);
        s.removeListener (this);
    }

    void parameterChanged (const juce::String& parameterID, float) override
    {
        if (parameterID == "phraseBars" || parameterID == "loopStart" || parameterID == "loopEnd"
            || parameterID == pid)
            refreshRangeAndValue();
    }

    void sliderValueChanged (juce::Slider*) override
    {
        if (updatingFromParam)
            return;

        const int steps = juce::jmax (1, phrase::readPhraseStepCount (ap));
        int v = (int) std::lround (s.getValue());
        v = juce::jlimit (1, steps, v);

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (ap.getParameter (pid)))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) v));
    }

private:
    void refreshRangeAndValue()
    {
        const int steps = juce::jmax (1, phrase::readPhraseStepCount (ap));
        s.setRange (1.0, (double) steps, 1.0);

        updatingFromParam = true;
        int v = 1;
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (ap.getParameter (pid)))
            v = (int) ip->get();
        v = juce::jlimit (1, steps, v);
        s.setValue ((double) v, juce::dontSendNotification);
        updatingFromParam = false;
    }

    juce::AudioProcessorValueTreeState& ap;
    juce::Slider& s;
    juce::String pid;
    bool updatingFromParam = false;
};

} // namespace bridge
