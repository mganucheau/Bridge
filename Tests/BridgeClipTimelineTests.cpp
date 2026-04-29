#include <JuceHeader.h>
#include "bridge_clip/BridgeClipTimeline.h"

struct BridgeClipTimelineXmlRoundTripTest : public juce::UnitTest
{
    BridgeClipTimelineXmlRoundTripTest()
        : juce::UnitTest ("Bridge clip timeline XML round-trip", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("toXml / fromXml preserves notes and microPpq");

        BridgeClipTimeline clip;
        clip.notes.push_back ({ 0, 0.0, 60, 100, 0.25, 0 });
        BridgeClipNote g;
        g.stepIndex0 = 3;
        g.microPpq   = 15.75;
        g.pitch      = 62;
        g.velocity   = 80;
        g.lengthPpq  = 0.5;
        g.setGhost (true);
        clip.notes.push_back (g);
        clip.sortByStepAndPitch();

        juce::XmlElement root ("Root");
        clip.toXml (root, "PianoClip");
        expect (root.getChildByName ("PianoClip") != nullptr);

        BridgeClipTimeline loaded;
        expect (loaded.fromXml (root, "PianoClip"));
        expectEquals ((int) loaded.notes.size(), 2);
        expectEquals ((int) loaded.notes[0].stepIndex0, 0);
        expectEquals ((int) loaded.notes[0].pitch, 60);
        expectEquals ((int) loaded.notes[1].stepIndex0, 3);
        expectWithinAbsoluteError (loaded.notes[1].microPpq, 15.75, 1.0e-6);
        expect (loaded.notes[1].isGhost());
    }
};

/** Same horizontal mapping as BridgeMidiClipEditor::paintNotes (ppq per 16th step). */
struct ClipNoteHorizontalStepCenterTest : public juce::UnitTest
{
    ClipNoteHorizontalStepCenterTest()
        : juce::UnitTest ("Bridge clip note step center (microPpq)", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("fractional step index from microPpq");

        constexpr double ppqPerStep = 0.25;
        BridgeClipNote n;
        n.stepIndex0 = 2;
        n.microPpq   = ppqPerStep * 0.5;
        const float stepCenter = (float) n.stepIndex0 + (float) (n.microPpq / ppqPerStep);
        expectWithinAbsoluteError ((double) stepCenter, 2.5, 1.0e-5);
    }
};

static BridgeClipTimelineXmlRoundTripTest bridgeClipTimelineXmlRoundTripTest;
static ClipNoteHorizontalStepCenterTest clipNoteHorizontalStepCenterTest;
