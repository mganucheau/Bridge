#include <JuceHeader.h>
#include "BridgeProcessor.h"

struct ProcessorStateRoundTripTest : public juce::UnitTest
{
    ProcessorStateRoundTripTest()
        : juce::UnitTest ("BridgeProcessor state round-trip", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("Binary state survives get / set");

        BridgeProcessor proc;

        juce::MemoryBlock data;
        proc.getStateInformation (data);
        expectGreaterThan ((int) data.getSize(), 32);

        auto xmlIn = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize());
        expect (xmlIn != nullptr);
        expect (xmlIn->hasTagName ("BridgeState"));
        const int verIn = xmlIn->getIntAttribute ("bridgeVersion", 0);
        expectGreaterThan (verIn, 0);

        proc.setStateInformation (data.getData(), (int) data.getSize());

        juce::MemoryBlock data2;
        proc.getStateInformation (data2);

        auto xmlOut = juce::AudioProcessor::getXmlFromBinary (data2.getData(), (int) data2.getSize());
        expect (xmlOut != nullptr);
        expect (xmlOut->hasTagName ("BridgeState"));
        expectEquals (xmlOut->getIntAttribute ("bridgeVersion", 0), verIn);
    }
};

static ProcessorStateRoundTripTest processorStateRoundTripTest;
