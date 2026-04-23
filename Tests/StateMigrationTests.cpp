#include <JuceHeader.h>
#include "BridgeProcessor.h"

namespace
{
void renameParamIdRecursive (juce::XmlElement& node, const juce::String& fromId, const juce::String& toId)
{
    if (node.hasTagName ("PARAM") && node.getStringAttribute ("id") == fromId)
        node.setAttribute ("id", toId);

    for (auto* ch : node.getChildIterator())
        renameParamIdRecursive (*ch, fromId, toId);
}

void setParamValueRecursive (juce::XmlElement& node, const juce::String& paramId, const juce::String& valueStr)
{
    if (node.hasTagName ("PARAM") && node.getStringAttribute ("id") == paramId)
    {
        node.setAttribute ("value", valueStr);
        return;
    }
    for (auto* ch : node.getChildIterator())
        setParamValueRecursive (*ch, paramId, valueStr);
}

juce::XmlElement* findChildWithTag (juce::XmlElement& root, const juce::String& tag)
{
    for (auto* ch : root.getChildIterator())
        if (ch->getTagName() == tag)
            return ch;
    return nullptr;
}
} // namespace

struct StateMigrationTests final : public juce::UnitTest
{
    StateMigrationTests()
        : juce::UnitTest ("State migration (bridgeVersion < 10 legacy ids)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("v9 piano: legacy pocket + presence map into hold + sustain");

        BridgeProcessor proc;
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        expect (xml != nullptr);

        xml->setAttribute ("bridgeVersion", 9);
        auto* pianoXml = findChildWithTag (*xml, proc.apvtsPiano.state.getType().toString());
        expect (pianoXml != nullptr);
        renameParamIdRecursive (*pianoXml, "hold", "pocket");
        renameParamIdRecursive (*pianoXml, "sustain", "presence");
        setParamValueRecursive (*pianoXml, "pocket", "0.37");
        setParamValueRecursive (*pianoXml, "presence", "0.88");

        juce::MemoryBlock patched;
        juce::AudioProcessor::copyXmlToBinary (*xml, patched);

        BridgeProcessor proc2;
        proc2.setStateInformation (patched.getData(), (int) patched.getSize());

        expectWithinAbsoluteError ((double) *proc2.apvtsPiano.getRawParameterValue ("hold"), 0.37, 0.02);
        expectWithinAbsoluteError ((double) *proc2.apvtsPiano.getRawParameterValue ("sustain"), 0.88, 0.02);

        beginTest ("v10 piano: legacy pocket / presence in XML are not migrated (hold/sustain stay at layout defaults)");

        BridgeProcessor procFresh;
        juce::MemoryBlock mb2;
        procFresh.getStateInformation (mb2);
        auto xml2 = juce::AudioProcessor::getXmlFromBinary (mb2.getData(), (int) mb2.getSize());
        expect (xml2 != nullptr);
        xml2->setAttribute ("bridgeVersion", 10);
        auto* pianoXml2 = findChildWithTag (*xml2, procFresh.apvtsPiano.state.getType().toString());
        expect (pianoXml2 != nullptr);
        renameParamIdRecursive (*pianoXml2, "hold", "pocket");
        renameParamIdRecursive (*pianoXml2, "sustain", "presence");
        setParamValueRecursive (*pianoXml2, "pocket", "0.01");
        setParamValueRecursive (*pianoXml2, "presence", "0.99");

        juce::MemoryBlock patched2;
        juce::AudioProcessor::copyXmlToBinary (*xml2, patched2);

        BridgeProcessor proc3;
        proc3.setStateInformation (patched2.getData(), (int) patched2.getSize());
        expectWithinAbsoluteError ((double) *proc3.apvtsPiano.getRawParameterValue ("hold"), 0.0, 0.06);
        expectWithinAbsoluteError ((double) *proc3.apvtsPiano.getRawParameterValue ("sustain"), 0.0, 0.02);

        beginTest ("v9 main: legacy pocket maps to hold");

        BridgeProcessor m0;
        juce::MemoryBlock mbM;
        m0.getStateInformation (mbM);
        auto xmlM = juce::AudioProcessor::getXmlFromBinary (mbM.getData(), (int) mbM.getSize());
        expect (xmlM != nullptr);
        xmlM->setAttribute ("bridgeVersion", 9);
        auto* mainXml = findChildWithTag (*xmlM, m0.apvtsMain.state.getType().toString());
        expect (mainXml != nullptr);
        renameParamIdRecursive (*mainXml, "hold", "pocket");
        setParamValueRecursive (*mainXml, "pocket", "0.22");

        juce::MemoryBlock mbOut;
        juce::AudioProcessor::copyXmlToBinary (*xmlM, mbOut);
        BridgeProcessor m1;
        m1.setStateInformation (mbOut.getData(), (int) mbOut.getSize());
        expectWithinAbsoluteError ((double) *m1.apvtsMain.getRawParameterValue ("hold"), 0.22, 0.02);

        beginTest ("v10 piano: legacy perform on maps to jam interval 1 bar");

        BridgeProcessor pj0;
        juce::MemoryBlock mbJ;
        pj0.getStateInformation (mbJ);
        auto xmlJ = juce::AudioProcessor::getXmlFromBinary (mbJ.getData(), (int) mbJ.getSize());
        expect (xmlJ != nullptr);
        xmlJ->setAttribute ("bridgeVersion", 10);
        auto* pianoXmlJ = findChildWithTag (*xmlJ, pj0.apvtsPiano.state.getType().toString());
        expect (pianoXmlJ != nullptr);
        auto* perf = new juce::XmlElement ("PARAM");
        perf->setAttribute ("id", "perform");
        perf->setAttribute ("value", "1.0");
        pianoXmlJ->addChildElement (perf);

        juce::MemoryBlock mbJout;
        juce::AudioProcessor::copyXmlToBinary (*xmlJ, mbJout);
        BridgeProcessor pj1;
        pj1.setStateInformation (mbJout.getData(), (int) mbJout.getSize());
        if (auto* jam = dynamic_cast<juce::AudioParameterChoice*> (pj1.apvtsPiano.getParameter ("jamInterval")))
            expectEquals (jam->getIndex(), 3);
        else
            expect (false, "jamInterval missing on piano APVTS");
    }
};

static StateMigrationTests stateMigrationTests;
