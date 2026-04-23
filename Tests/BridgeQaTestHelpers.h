#pragma once

#include <JuceHeader.h>

/** Minimal play head so BridgeProcessor::processBlock sees transport as running. */
struct BridgeQaFakePlayHead : public juce::AudioPlayHead
{
    double ppqPosition = 0.0;
    double bpm = 120.0;
    bool playing = true;

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setIsPlaying (playing);
        info.setIsRecording (false);
        info.setIsLooping (false);
        info.setBpm (bpm);
        info.setPpqPosition (ppqPosition);
        const double beatsPerSec = bpm / 60.0;
        if (beatsPerSec > 0.0)
            info.setTimeInSeconds (ppqPosition / beatsPerSec);
        else
            info.setTimeInSeconds (0.0);
        return info;
    }
};

inline int bridgeQaCountControllerValue (const juce::MidiBuffer& midi,
                                           int channel1Based,
                                           int controllerNumber,
                                           int value)
{
    int n = 0;
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isControllerOfType (controllerNumber) && m.getControllerValue() == value
            && m.getChannel() == channel1Based)
            ++n;
    }
    return n;
}

inline bool bridgeQaHasNoteOff (const juce::MidiBuffer& midi, int channel1Based, int note)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isNoteOff() && m.getNoteNumber() == note && m.getChannel() == channel1Based)
            return true;
    }
    return false;
}

/** Sets a linear 0–1 APVTS float/bool host value (`setValueNotifyingHost` normalized). */
inline void bridgeQaSetFloatParam01 (juce::AudioProcessorValueTreeState& apvts, const char* id, float v01)
{
    v01 = juce::jlimit (0.0f, 1.0f, v01);
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (v01);
}

inline void bridgeQaSetIntParam (juce::AudioProcessorValueTreeState& apvts, const char* id, int v)
{
    if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (id)))
        ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) v));
}
