#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "BridgePhrase.h"

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

        beginTest ("v11 → v12: state without new arrangement / drift / articulation params loads with layout defaults");

        // Build a v11-style state by rewriting the version attribute on an otherwise-current snapshot.
        BridgeProcessor v11src;
        juce::MemoryBlock mbV11;
        v11src.getStateInformation (mbV11);
        auto xmlV11 = juce::AudioProcessor::getXmlFromBinary (mbV11.getData(), (int) mbV11.getSize());
        expect (xmlV11 != nullptr);
        xmlV11->setAttribute ("bridgeVersion", 11);

        // Strip the v12 PARAMs so the state looks like an older save that never knew them.
        auto strip = [] (juce::XmlElement* node, std::initializer_list<const char*> ids)
        {
            if (node == nullptr) return;
            std::vector<juce::XmlElement*> doomed;
            for (auto* ch : node->getChildIterator())
                if (ch->hasTagName ("PARAM"))
                    for (auto* id : ids)
                        if (ch->getStringAttribute ("id") == id)
                            doomed.push_back (ch);
            for (auto* d : doomed)
                node->removeChildElement (d, true);
        };
        strip (findChildWithTag (*xmlV11, v11src.apvtsMain.state.getType().toString()),
               { "arrSection", "sectionIntensity" });
        strip (findChildWithTag (*xmlV11, v11src.apvtsDrums.state.getType().toString()),
               { "lockKick", "lockSnare", "lockHats", "lockPerc", "life", "hatOpen", "velShape" });
        strip (findChildWithTag (*xmlV11, v11src.apvtsBass.state.getType().toString()),
               { "life", "melody", "followRhythm", "velShape", "slideAmt" });
        strip (findChildWithTag (*xmlV11, v11src.apvtsPiano.state.getType().toString()),
               { "life", "melody", "followRhythm", "velShape", "voicingSpread" });
        strip (findChildWithTag (*xmlV11, v11src.apvtsGuitar.state.getType().toString()),
               { "life", "melody", "followRhythm", "velShape", "palmMute", "strumIntensity" });

        juce::MemoryBlock mbV11Out;
        juce::AudioProcessor::copyXmlToBinary (*xmlV11, mbV11Out);
        BridgeProcessor v12dst;
        v12dst.setStateInformation (mbV11Out.getData(), (int) mbV11Out.getSize());

        if (auto* arr = dynamic_cast<juce::AudioParameterChoice*> (v12dst.apvtsMain.getParameter ("arrSection")))
            expectEquals (arr->getIndex(), 1);
        expectWithinAbsoluteError ((double) *v12dst.apvtsMain.getRawParameterValue ("sectionIntensity"), 0.0, 0.02);

        for (auto* id : { "lockKick", "lockSnare", "lockHats", "lockPerc" })
            expectWithinAbsoluteError ((double) *v12dst.apvtsDrums.getRawParameterValue (id), 0.0, 0.02);
        expectWithinAbsoluteError ((double) *v12dst.apvtsDrums.getRawParameterValue ("life"),    0.0, 0.02);
        expectWithinAbsoluteError ((double) *v12dst.apvtsDrums.getRawParameterValue ("hatOpen"), 0.0, 0.02);

        for (auto* apvts : { &v12dst.apvtsBass, &v12dst.apvtsPiano, &v12dst.apvtsGuitar })
        {
            expectWithinAbsoluteError ((double) *apvts->getRawParameterValue ("life"),         0.0, 0.02);
            expectWithinAbsoluteError ((double) *apvts->getRawParameterValue ("melody"),       0.0, 0.02);
            expectWithinAbsoluteError ((double) *apvts->getRawParameterValue ("followRhythm"), 0.0, 0.02);
        }
        expectWithinAbsoluteError ((double) *v12dst.apvtsBass.getRawParameterValue ("slideAmt"),         0.0, 0.02);
        expectWithinAbsoluteError ((double) *v12dst.apvtsPiano.getRawParameterValue ("voicingSpread"),   0.0, 0.02);
        expectWithinAbsoluteError ((double) *v12dst.apvtsGuitar.getRawParameterValue ("palmMute"),       0.0, 0.02);
        expectWithinAbsoluteError ((double) *v12dst.apvtsGuitar.getRawParameterValue ("strumIntensity"), 0.0, 0.02);

        beginTest ("v12 round trip: explicit non-default values for the new params persist");

        BridgeProcessor src;
        if (auto* arr = dynamic_cast<juce::AudioParameterChoice*> (src.apvtsMain.getParameter ("arrSection")))
            arr->setValueNotifyingHost (arr->getNormalisableRange().convertTo0to1 (2.0f)); // Chorus
        if (auto* p = src.apvtsMain.getParameter ("sectionIntensity")) p->setValueNotifyingHost (0.81f);
        if (auto* p = src.apvtsDrums.getParameter ("lockKick"))        p->setValueNotifyingHost (1.0f);
        if (auto* p = src.apvtsDrums.getParameter ("life"))            p->setValueNotifyingHost (0.6f);
        if (auto* p = src.apvtsBass.getParameter ("followRhythm"))     p->setValueNotifyingHost (0.7f);
        if (auto* p = src.apvtsGuitar.getParameter ("palmMute"))       p->setValueNotifyingHost (0.4f);

        juce::MemoryBlock mbRT;
        src.getStateInformation (mbRT);
        BridgeProcessor dst;
        dst.setStateInformation (mbRT.getData(), (int) mbRT.getSize());

        if (auto* arr = dynamic_cast<juce::AudioParameterChoice*> (dst.apvtsMain.getParameter ("arrSection")))
            expectEquals (arr->getIndex(), 2);
        expectWithinAbsoluteError ((double) *dst.apvtsMain.getRawParameterValue ("sectionIntensity"), 0.81, 0.02);
        expect (*dst.apvtsDrums.getRawParameterValue ("lockKick") > 0.5f);
        expectWithinAbsoluteError ((double) *dst.apvtsDrums.getRawParameterValue ("life"),         0.6, 0.02);
        expectWithinAbsoluteError ((double) *dst.apvtsBass.getRawParameterValue ("followRhythm"),  0.7, 0.02);
        expectWithinAbsoluteError ((double) *dst.apvtsGuitar.getRawParameterValue ("palmMute"),    0.4, 0.02);

        beginTest ("v14 → v15: phraseBars normalized 1.0 (old 16 bars) becomes 8-bar choice");

        BridgeProcessor pv15;
        juce::MemoryBlock mbV15;
        pv15.getStateInformation (mbV15);
        auto xmlV15 = juce::AudioProcessor::getXmlFromBinary (mbV15.getData(), (int) mbV15.getSize());
        expect (xmlV15 != nullptr);
        xmlV15->setAttribute ("bridgeVersion", 14);
        auto* mainV15 = findChildWithTag (*xmlV15, pv15.apvtsMain.state.getType().toString());
        expect (mainV15 != nullptr);
        setParamValueRecursive (*mainV15, "phraseBars", "1");

        juce::MemoryBlock mbV15Out;
        juce::AudioProcessor::copyXmlToBinary (*xmlV15, mbV15Out);
        BridgeProcessor pv15loaded;
        pv15loaded.setStateInformation (mbV15Out.getData(), (int) mbV15Out.getSize());
        if (auto* ph = dynamic_cast<juce::AudioParameterChoice*> (pv15loaded.apvtsMain.getParameter ("phraseBars")))
        {
            expectEquals (ph->getIndex(), 2);
            expectEquals (bridge::phrase::phraseBarsFromChoiceIndex (ph->getIndex()), 8);
        }
        else
            expect (false, "phraseBars missing");
    }
};

static StateMigrationTests stateMigrationTests;
