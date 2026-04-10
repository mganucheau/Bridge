#include "BridgeProcessor.h"
#include "BridgeEditor.h"
#include "bootsy/BootsyStylePresets.h"
#include "stevie/StevieStylePresets.h"
#include "paul/PaulStylePresets.h"

namespace
{
static constexpr const char* kAnimalStateId = "AnimalDrummer";
static constexpr const char* kBootsyStateId = "Bootsy";
}

juce::AudioProcessorValueTreeState::ParameterLayout BridgeProcessor::buildMainLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterBool> ("animalOn",  "Animal on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("bootsyOn",  "Bootsy on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("stevieOn",  "Stevie on",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> ("paulOn",    "Paul on",    true));
    return layout;
}

juce::AudioProcessorValueTreeState::ParameterLayout BridgeProcessor::buildAnimalLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray styleNames;
    for (int i = 0; i < NUM_STYLES; ++i)
        styleNames.add (STYLE_NAMES[i]);

    layout.add (std::make_unique<juce::AudioParameterChoice> ("style", "Style", styleNames, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.85f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.15f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart",   "Loop Start",  1,     NUM_STEPS, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd",     "Loop End",      1,     NUM_STEPS, NUM_STEPS));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopWidthLock", "Loop Width Lock", false));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("perform",     "Perform",     false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("tickerSpeed", "Ticker",
                                                              juce::StringArray { "2", "1", "1/2" }, 1));

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> ("mute_" + juce::String (drum),
                                                                "Mute " + juce::String (DRUM_NAMES[drum]),
                                                                false));
        layout.add (std::make_unique<juce::AudioParameterBool> ("solo_" + juce::String (drum),
                                                                "Solo " + juce::String (DRUM_NAMES[drum]),
                                                                false));
    }

    return layout;
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeMelodicLayout (int defaultMidiChannel)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt>   ("style",       "Style",
                                                             0, BootsyPreset::NUM_STYLES - 1, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("scale",       "Scale",
                                                             0, BootsyPreset::NUM_SCALES - 1, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("rootNote",    "Root Note",   0, 11, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("octave",      "Octave",      1, 4, 2));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("temperature", "Temperature", 0.01f, 2.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",     0.0f,  1.0f, 0.70f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("swing",       "Swing",       0.0f,  1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("humanize",    "Humanize",    0.0f,  1.0f, 0.20f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pocket",      "Pocket",      0.0f,  1.0f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity",    "Velocity",    0.0f,  1.0f, 0.85f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fillRate",    "Fill Rate",   0.0f,  1.0f, 0.15f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("complexity",  "Complexity",  0.0f,  1.0f, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ghostAmount", "Ghost",       0.0f,  1.0f, 0.70f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("staccato",    "Staccato",    0.0f,  1.0f, 0.20f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("patternLen",  "Pattern Len", 4,     16,   16));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("locked",      "Locked",      false));
    layout.add (std::make_unique<juce::AudioParameterInt> ("midiChannel",  "MIDI Channel", 1, 16, defaultMidiChannel));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("phraseBars", "Bar Grid",
                                                              juce::StringArray { "4 bars", "8 bars", "16 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopStart",   "Loop Start",   1, BootsyPreset::NUM_STEPS, 1));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("loopEnd",     "Loop End",     1, BootsyPreset::NUM_STEPS, BootsyPreset::NUM_STEPS));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("loopWidthLock", "Loop Width Lock", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("tickerSpeed", "Ticker",
                                                              juce::StringArray { "2", "1", "1/2" }, 1));

    return layout;
}

BridgeProcessor::BridgeProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvtsMain   (*this, nullptr, "BridgeMain", buildMainLayout()),
      apvtsAnimal (*this, nullptr, kAnimalStateId, buildAnimalLayout()),
      apvtsBootsy (*this, nullptr, kBootsyStateId, makeMelodicLayout (1)),
      apvtsStevie (*this, nullptr, "Stevie", makeMelodicLayout (2)),
      apvtsPaul   (*this, nullptr, "Paul", makeMelodicLayout (3))
{
    syncAnimalEngineFromAPVTS();
    syncBootsyEngineFromAPVTS();
    syncStevieEngineFromAPVTS();
    syncPaulEngineFromAPVTS();

    drumEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyAnimalPatternChanged();
    };

    bassEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyBootsyPatternChanged();
    };

    pianoEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifySteviePatternChanged();
    };

    guitarEngine.onPatternChanged = [this]
    {
        if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
            ed->notifyPaulPatternChanged();
    };
}

BridgeProcessor::~BridgeProcessor() {}

juce::AudioProcessorEditor* BridgeProcessor::createEditor()
{
    return new BridgeEditor (*this);
}

void BridgeProcessor::prepareToPlay (double sr, int spb)
{
    sampleRate = sr;
    samplesPerBlock = spb;

    midiSampleClock = 0;

    animalLastProcessedStep = -1;
    animalPendingNoteOffs.clear();
    drumEngine.generatePattern (false);
    drumEngine.setPlaybackSamplesPerStep (sampleRate * 60.0 / 120.0 / 4.0);
    syncAnimalEngineFromAPVTS();
    drumEngine.rebuildGridPreview();

    bootsyLastProcessedStep = -1;
    bootsyPendingNoteOffs.clear();
    syncBootsyEngineFromAPVTS();
    bassEngine.generatePattern();
    bassEngine.rebuildGridPreview();

    stevieLastProcessedStep = -1;
    steviePendingNoteOffs.clear();
    syncStevieEngineFromAPVTS();
    pianoEngine.generatePattern();
    pianoEngine.rebuildGridPreview();

    paulLastProcessedStep = -1;
    paulPendingNoteOffs.clear();
    syncPaulEngineFromAPVTS();
    guitarEngine.generatePattern();
    guitarEngine.rebuildGridPreview();
}

const DrumPattern& BridgeProcessor::getPatternForGrid() const
{
    if (drumEngine.isPerformBoost())
        return drumEngine.getPattern();
    return drumEngine.getPatternForGrid();
}

void BridgeProcessor::getAnimalLoopBounds (int& loopStart, int& loopEnd) const
{
    loopStart = (int) *apvtsAnimal.getRawParameterValue ("loopStart");
    loopEnd   = (int) *apvtsAnimal.getRawParameterValue ("loopEnd");
    loopStart = juce::jlimit (1, NUM_STEPS, loopStart);
    loopEnd   = juce::jlimit (1, NUM_STEPS, loopEnd);
    if (loopEnd < loopStart)
        std::swap (loopStart, loopEnd);
}

void BridgeProcessor::getBootsyLoopBounds (int& loopStart, int& loopEnd) const
{
    loopStart = (int) *apvtsBootsy.getRawParameterValue ("loopStart");
    loopEnd   = (int) *apvtsBootsy.getRawParameterValue ("loopEnd");
    loopStart = juce::jlimit (1, BootsyPreset::NUM_STEPS, loopStart);
    loopEnd   = juce::jlimit (1, BootsyPreset::NUM_STEPS, loopEnd);
    if (loopEnd < loopStart)
        std::swap (loopStart, loopEnd);
}

void BridgeProcessor::syncAnimalEngineFromAPVTS()
{
    if (auto* styleParam = dynamic_cast<juce::AudioParameterChoice*> (apvtsAnimal.getParameter ("style")))
        drumEngine.setStyle (styleParam->getIndex());
    else
        drumEngine.setStyle ((int) apvtsAnimal.getRawParameterValue ("style")->load());

    drumEngine.setTemperature (1.0f);
    drumEngine.setDensity    ((float)*apvtsAnimal.getRawParameterValue ("density"));
    drumEngine.setSwing      ((float)*apvtsAnimal.getRawParameterValue ("swing"));
    drumEngine.setHumanize   ((float)*apvtsAnimal.getRawParameterValue ("humanize"));
    drumEngine.setVelocity   ((float)*apvtsAnimal.getRawParameterValue ("velocity"));
    drumEngine.setFillRate   ((float)*apvtsAnimal.getRawParameterValue ("fillRate"));
    drumEngine.setComplexity ((float)*apvtsAnimal.getRawParameterValue ("complexity"));
    drumEngine.setPatternLen (NUM_STEPS);

    animalPerformMode = ((bool) *apvtsAnimal.getRawParameterValue ("perform"));
    drumEngine.setPerformBoost (animalPerformMode);

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsAnimal.getParameter ("tickerSpeed")))
        animalTickerRate = animalTickerRateForChoiceIndex (tr->getIndex());
    else
        animalTickerRate = animalTickerRateForChoiceIndex ((int) *apvtsAnimal.getRawParameterValue ("tickerSpeed"));

    drumEngine.setPhraseBars (4);

    animalAnySolo = false;
    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        animalLaneMute[(size_t) drum] = ((bool) *apvtsAnimal.getRawParameterValue ("mute_" + juce::String (drum)));
        animalLaneSolo[(size_t) drum] = ((bool) *apvtsAnimal.getRawParameterValue ("solo_" + juce::String (drum)));
        animalAnySolo = animalAnySolo || animalLaneSolo[(size_t) drum];
    }
}

double BridgeProcessor::animalTickerRateForChoiceIndex (int choiceIndex)
{
    switch (juce::jlimit (0, 2, choiceIndex))
    {
        case 0: return 2.0;
        case 2: return 0.5;
        default: return 1.0;
    }
}

double BridgeProcessor::bootsyTickerRateForChoiceIndex (int choiceIndex)
{
    switch (juce::jlimit (0, 2, choiceIndex))
    {
        case 0: return 2.0;
        case 2: return 0.5;
        default: return 1.0;
    }
}

bool BridgeProcessor::isAnimalLaneAudible (int drum) const
{
    if (drum < 0 || drum >= NUM_DRUMS) return false;
    return animalAnySolo ? animalLaneSolo[(size_t) drum]
                         : ! animalLaneMute[(size_t) drum];
}

void BridgeProcessor::rebuildAnimalGridPreview()
{
    syncAnimalEngineFromAPVTS();
    drumEngine.rebuildGridPreview();
}

void BridgeProcessor::randomizeAnimalSettings()
{
    juce::Random r;

    if (auto* p = apvtsAnimal.getParameter ("style"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (NUM_STYLES)));

    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsAnimal.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };

    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);

    if (auto* p = apvtsAnimal.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));

    drumEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::triggerAnimalGenerate()
{
    randomizeAnimalSettings();
    syncAnimalEngineFromAPVTS();
    drumEngine.generatePattern (animalPerformMode);
}

void BridgeProcessor::triggerAnimalFill (int fromStep)
{
    animalFillQueued   = true;
    animalFillFromStep = fromStep;
}

void BridgeProcessor::syncBootsyEngineFromAPVTS()
{
    bassEngine.setStyle       ((int)  *apvtsBootsy.getRawParameterValue ("style"));
    bassEngine.setScale       ((int)  *apvtsBootsy.getRawParameterValue ("scale"));
    bassEngine.setRootNote    ((int)  *apvtsBootsy.getRawParameterValue ("rootNote"));
    bassEngine.setOctave      ((int)  *apvtsBootsy.getRawParameterValue ("octave"));
    bassEngine.setTemperature ((float)*apvtsBootsy.getRawParameterValue ("temperature"));
    bassEngine.setDensity     ((float)*apvtsBootsy.getRawParameterValue ("density"));
    bassEngine.setSwing       ((float)*apvtsBootsy.getRawParameterValue ("swing"));
    bassEngine.setHumanize    ((float)*apvtsBootsy.getRawParameterValue ("humanize"));
    bassEngine.setPocket      ((float)*apvtsBootsy.getRawParameterValue ("pocket"));
    bassEngine.setVelocity    ((float)*apvtsBootsy.getRawParameterValue ("velocity"));
    bassEngine.setFillRate    ((float)*apvtsBootsy.getRawParameterValue ("fillRate"));
    bassEngine.setComplexity  ((float)*apvtsBootsy.getRawParameterValue ("complexity"));
    bassEngine.setGhostAmount ((float)*apvtsBootsy.getRawParameterValue ("ghostAmount"));
    bassEngine.setStaccato    ((float)*apvtsBootsy.getRawParameterValue ("staccato"));
    bassEngine.setPatternLen  (BootsyPreset::NUM_STEPS);
    bassEngine.setLocked      ((bool) *apvtsBootsy.getRawParameterValue ("locked"));

    bootsyMidiChannel = juce::jlimit (1, 16, (int)*apvtsBootsy.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsBootsy.getRawParameterValue ("phraseBars");
    bassEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsBootsy.getParameter ("tickerSpeed")))
        bootsyTickerRate = bootsyTickerRateForChoiceIndex (tr->getIndex());
    else
        bootsyTickerRate = bootsyTickerRateForChoiceIndex ((int) *apvtsBootsy.getRawParameterValue ("tickerSpeed"));
}

void BridgeProcessor::triggerBootsyGenerate()
{
    randomizeBootsySettings();
    syncBootsyEngineFromAPVTS();
    bassEngine.generatePattern();
}

void BridgeProcessor::applyBootsyStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, BootsyPreset::NUM_STYLES - 1, styleIndex);
    if (auto* p = apvtsBootsy.getParameter ("style"))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) styleIndex));
    syncBootsyEngineFromAPVTS();
    bassEngine.generatePattern();
}

void BridgeProcessor::randomizeBootsySettings()
{
    juce::Random r;

    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsBootsy.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };

    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("pocket",     0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);

    if (auto* p = apvtsBootsy.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));

    bassEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::rebuildBootsyGridPreview()
{
    syncBootsyEngineFromAPVTS();
    bassEngine.rebuildGridPreview();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyBootsyPatternChanged();
}

void BridgeProcessor::triggerBootsyFill (int fromStep)
{
    bootsyFillQueued   = true;
    bootsyFillFromStep = fromStep;
}

void BridgeProcessor::getStevieLoopBounds (int& loopStart, int& loopEnd) const
{
    loopStart = (int) *apvtsStevie.getRawParameterValue ("loopStart");
    loopEnd   = (int) *apvtsStevie.getRawParameterValue ("loopEnd");
    loopStart = juce::jlimit (1, SteviePreset::NUM_STEPS, loopStart);
    loopEnd   = juce::jlimit (1, SteviePreset::NUM_STEPS, loopEnd);
    if (loopEnd < loopStart)
        std::swap (loopStart, loopEnd);
}

void BridgeProcessor::getPaulLoopBounds (int& loopStart, int& loopEnd) const
{
    loopStart = (int) *apvtsPaul.getRawParameterValue ("loopStart");
    loopEnd   = (int) *apvtsPaul.getRawParameterValue ("loopEnd");
    loopStart = juce::jlimit (1, PaulPreset::NUM_STEPS, loopStart);
    loopEnd   = juce::jlimit (1, PaulPreset::NUM_STEPS, loopEnd);
    if (loopEnd < loopStart)
        std::swap (loopStart, loopEnd);
}

void BridgeProcessor::syncStevieEngineFromAPVTS()
{
    pianoEngine.setStyle       ((int)  *apvtsStevie.getRawParameterValue ("style"));
    pianoEngine.setScale       ((int)  *apvtsStevie.getRawParameterValue ("scale"));
    pianoEngine.setRootNote    ((int)  *apvtsStevie.getRawParameterValue ("rootNote"));
    pianoEngine.setOctave      ((int)  *apvtsStevie.getRawParameterValue ("octave"));
    pianoEngine.setTemperature ((float)*apvtsStevie.getRawParameterValue ("temperature"));
    pianoEngine.setDensity     ((float)*apvtsStevie.getRawParameterValue ("density"));
    pianoEngine.setSwing       ((float)*apvtsStevie.getRawParameterValue ("swing"));
    pianoEngine.setHumanize    ((float)*apvtsStevie.getRawParameterValue ("humanize"));
    pianoEngine.setPocket      ((float)*apvtsStevie.getRawParameterValue ("pocket"));
    pianoEngine.setVelocity    ((float)*apvtsStevie.getRawParameterValue ("velocity"));
    pianoEngine.setFillRate    ((float)*apvtsStevie.getRawParameterValue ("fillRate"));
    pianoEngine.setComplexity  ((float)*apvtsStevie.getRawParameterValue ("complexity"));
    pianoEngine.setGhostAmount ((float)*apvtsStevie.getRawParameterValue ("ghostAmount"));
    pianoEngine.setStaccato    ((float)*apvtsStevie.getRawParameterValue ("staccato"));
    pianoEngine.setPatternLen  (SteviePreset::NUM_STEPS);
    pianoEngine.setLocked      ((bool) *apvtsStevie.getRawParameterValue ("locked"));

    stevieMidiChannel = juce::jlimit (1, 16, (int)*apvtsStevie.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsStevie.getRawParameterValue ("phraseBars");
    pianoEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsStevie.getParameter ("tickerSpeed")))
        stevieTickerRate = bootsyTickerRateForChoiceIndex (tr->getIndex());
    else
        stevieTickerRate = bootsyTickerRateForChoiceIndex ((int) *apvtsStevie.getRawParameterValue ("tickerSpeed"));
}

void BridgeProcessor::syncPaulEngineFromAPVTS()
{
    guitarEngine.setStyle       ((int)  *apvtsPaul.getRawParameterValue ("style"));
    guitarEngine.setScale       ((int)  *apvtsPaul.getRawParameterValue ("scale"));
    guitarEngine.setRootNote    ((int)  *apvtsPaul.getRawParameterValue ("rootNote"));
    guitarEngine.setOctave      ((int)  *apvtsPaul.getRawParameterValue ("octave"));
    guitarEngine.setTemperature ((float)*apvtsPaul.getRawParameterValue ("temperature"));
    guitarEngine.setDensity     ((float)*apvtsPaul.getRawParameterValue ("density"));
    guitarEngine.setSwing       ((float)*apvtsPaul.getRawParameterValue ("swing"));
    guitarEngine.setHumanize    ((float)*apvtsPaul.getRawParameterValue ("humanize"));
    guitarEngine.setPocket      ((float)*apvtsPaul.getRawParameterValue ("pocket"));
    guitarEngine.setVelocity    ((float)*apvtsPaul.getRawParameterValue ("velocity"));
    guitarEngine.setFillRate    ((float)*apvtsPaul.getRawParameterValue ("fillRate"));
    guitarEngine.setComplexity  ((float)*apvtsPaul.getRawParameterValue ("complexity"));
    guitarEngine.setGhostAmount ((float)*apvtsPaul.getRawParameterValue ("ghostAmount"));
    guitarEngine.setStaccato    ((float)*apvtsPaul.getRawParameterValue ("staccato"));
    guitarEngine.setPatternLen  (PaulPreset::NUM_STEPS);
    guitarEngine.setLocked      ((bool) *apvtsPaul.getRawParameterValue ("locked"));

    paulMidiChannel = juce::jlimit (1, 16, (int)*apvtsPaul.getRawParameterValue ("midiChannel"));

    const int phraseChoice = (int)*apvtsPaul.getRawParameterValue ("phraseBars");
    guitarEngine.setPhraseBars (phraseChoice == 0 ? 4 : (phraseChoice == 1 ? 8 : 16));

    if (auto* tr = dynamic_cast<juce::AudioParameterChoice*> (apvtsPaul.getParameter ("tickerSpeed")))
        paulTickerRate = bootsyTickerRateForChoiceIndex (tr->getIndex());
    else
        paulTickerRate = bootsyTickerRateForChoiceIndex ((int) *apvtsPaul.getRawParameterValue ("tickerSpeed"));
}

void BridgeProcessor::randomizeStevieSettings()
{
    juce::Random r;
    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsStevie.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };
    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("pocket",     0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);
    if (auto* p = apvtsStevie.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));
    pianoEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::randomizePaulSettings()
{
    juce::Random r;
    auto randFloat = [&] (const char* id, float lo, float hi)
    {
        if (auto* p = apvtsPaul.getParameter (id))
            if (auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p))
                fp->setValueNotifyingHost (fp->getNormalisableRange().convertTo0to1 (lo + (hi - lo) * r.nextFloat()));
    };
    randFloat ("density",    0.35f, 1.0f);
    randFloat ("swing",      0.0f,  0.45f);
    randFloat ("humanize",   0.05f, 0.55f);
    randFloat ("pocket",     0.2f,  0.95f);
    randFloat ("velocity",   0.55f, 1.0f);
    randFloat ("fillRate",   0.05f, 0.45f);
    randFloat ("complexity", 0.2f,  0.95f);
    randFloat ("ghostAmount",0.35f, 1.0f);
    randFloat ("staccato",   0.05f, 0.55f);
    if (auto* p = apvtsPaul.getParameter ("tickerSpeed"))
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) r.nextInt (3)));
    guitarEngine.setSeed ((uint32_t) ((uint32_t) r.nextInt (0x7fffffff) ^ 0xA5A5A5A5u));
}

void BridgeProcessor::triggerStevieGenerate()
{
    randomizeStevieSettings();
    syncStevieEngineFromAPVTS();
    pianoEngine.generatePattern();
}

void BridgeProcessor::applyStevieStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, SteviePreset::NUM_STYLES - 1, styleIndex);
    if (auto* p = apvtsStevie.getParameter ("style"))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) styleIndex));
    syncStevieEngineFromAPVTS();
    pianoEngine.generatePattern();
}

void BridgeProcessor::rebuildStevieGridPreview()
{
    syncStevieEngineFromAPVTS();
    pianoEngine.rebuildGridPreview();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifySteviePatternChanged();
}

void BridgeProcessor::triggerStevieFill (int fromStep)
{
    stevieFillQueued   = true;
    stevieFillFromStep = fromStep;
}

void BridgeProcessor::triggerPaulGenerate()
{
    randomizePaulSettings();
    syncPaulEngineFromAPVTS();
    guitarEngine.generatePattern();
}

void BridgeProcessor::applyPaulStyleAndRegenerate (int styleIndex)
{
    styleIndex = juce::jlimit (0, PaulPreset::NUM_STYLES - 1, styleIndex);
    if (auto* p = apvtsPaul.getParameter ("style"))
        if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p))
            ip->setValueNotifyingHost (ip->getNormalisableRange().convertTo0to1 ((float) styleIndex));
    syncPaulEngineFromAPVTS();
    guitarEngine.generatePattern();
}

void BridgeProcessor::rebuildPaulGridPreview()
{
    syncPaulEngineFromAPVTS();
    guitarEngine.rebuildGridPreview();
    if (auto* ed = dynamic_cast<BridgeEditor*> (getActiveEditor()))
        ed->notifyPaulPatternChanged();
}

void BridgeProcessor::triggerPaulFill (int fromStep)
{
    paulFillQueued   = true;
    paulFillFromStep = fromStep;
}

void BridgeProcessor::flushPendingNoteOffs (juce::Array<PendingOff>& pending, juce::MidiBuffer& midi)
{
    for (int i = pending.size() - 1; i >= 0; --i)
    {
        auto& off = pending.getReference (i);
        midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), 0);
        pending.remove (i);
    }
}

void BridgeProcessor::scheduleAnimalNotesForStep (int globalStep, int wrappedStep, double samplesPerStep,
                                                   int sampleOffset, juce::MidiBuffer& midi)
{
    DrumStep hits;
    drumEngine.evaluateStepForPlayback (globalStep, wrappedStep, hits, samplesPerStep);

    for (int drum = 0; drum < NUM_DRUMS; ++drum)
    {
        if (! isAnimalLaneAudible (drum)) continue;
        if (!hits[drum].active) continue;

        int swingOff = drumEngine.getSwingOffset (wrappedStep, samplesPerStep);
        int humanOff = hits[(size_t) drum].timeShift;
        int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

        const int midiNote = juce::jlimit (1, 127, (int) DRUM_MIDI_NOTES[drum]);
        uint8 vel = hits[(size_t) drum].velocity;

        constexpr int midiChannel = 10;
        midi.addEvent (juce::MidiMessage::noteOn  (midiChannel, midiNote, vel), finalOff);

        int64 offAt = midiSampleClock + finalOff + (int64)(sampleRate * 0.05);
        animalPendingNoteOffs.add ({ midiChannel, midiNote, offAt });
    }
}

void BridgeProcessor::sendAnimalNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = animalPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = animalPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            animalPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processAnimalBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncAnimalEngineFromAPVTS();
    if (! drumEngine.isPerformBoost())
        drumEngine.rebuildGridPreview();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    animalLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    double samplesPerStep = samplesPerBeat / 4.0;
    drumEngine.setPlaybackSamplesPerStep (samplesPerStep);

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

    constexpr double ppqPerStep = 0.25;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getAnimalLoopBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int) std::floor ((double) globalStep * animalTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != animalLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != animalLastProcessedStep)
        {
            if (animalPerformMode)
            {
                syncAnimalEngineFromAPVTS();
                drumEngine.generatePattern (true);
            }

            if (animalFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, animalFillFromStep);
                drumEngine.generateFill (fillStart);
                animalFillQueued = false;
            }
            else if (drumEngine.shouldTriggerFill())
            {
                drumEngine.generateFill (juce::jmin (12, le0));
            }
        }

        animalCurrentStep.store (wrappedStep);
        animalCurrentVisualStep.store (visualWrapped);
        animalLastProcessedStep = globalStep;

        scheduleAnimalNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendAnimalNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::scheduleBootsyNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                   int sampleOffset, juce::MidiBuffer& midi)
{
    const BassHit& hit  = bassEngine.getStep (wrappedStep);
    if (!hit.active) return;

    int swingOff = bassEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

    uint8 vel    = hit.velocity;
    int   note   = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (bootsyMidiChannel, note, vel), finalOff);

    int durSamples = bassEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    bootsyPendingNoteOffs.add ({ bootsyMidiChannel, note, offAt });
}

void BridgeProcessor::sendBootsyNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = bootsyPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = bootsyPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            bootsyPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processBootsyBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncBootsyEngineFromAPVTS();
    bassEngine.rebuildGridPreview();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    bootsyLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    double samplesPerStep = samplesPerBeat / 4.0;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

    constexpr double ppqPerStep = 0.25;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getBootsyLoopBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * bootsyTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != bootsyLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != bootsyLastProcessedStep)
        {
            if (bootsyFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, bootsyFillFromStep);
                bassEngine.generateFill (fillStart);
                bootsyFillQueued = false;
            }
            else if (bassEngine.shouldTriggerFill())
            {
                bassEngine.generateFill (juce::jmin (12, le0));
            }
        }

        bootsyCurrentStep.store (wrappedStep);
        bootsyCurrentVisualStep.store (visualWrapped);
        bootsyLastProcessedStep = globalStep;

        scheduleBootsyNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendBootsyNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::scheduleStevieNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                  int sampleOffset, juce::MidiBuffer& midi)
{
    const PianoHit& hit = pianoEngine.getStep (wrappedStep);
    if (! hit.active) return;

    int swingOff = pianoEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

    uint8 vel  = hit.velocity;
    int   note = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (stevieMidiChannel, note, vel), finalOff);

    int durSamples = pianoEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    steviePendingNoteOffs.add ({ stevieMidiChannel, note, offAt });
}

void BridgeProcessor::sendStevieNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = steviePendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = steviePendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            steviePendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processStevieBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                          const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncStevieEngineFromAPVTS();
    pianoEngine.rebuildGridPreview();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    stevieLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    double samplesPerStep = samplesPerBeat / 4.0;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

    constexpr double ppqPerStep = 0.25;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getStevieLoopBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * stevieTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != stevieLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != stevieLastProcessedStep)
        {
            if (stevieFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, stevieFillFromStep);
                pianoEngine.generateFill (fillStart);
                stevieFillQueued = false;
            }
            else if (pianoEngine.shouldTriggerFill())
            {
                pianoEngine.generateFill (juce::jmin (12, le0));
            }
        }

        stevieCurrentStep.store (wrappedStep);
        stevieCurrentVisualStep.store (visualWrapped);
        stevieLastProcessedStep = globalStep;

        scheduleStevieNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendStevieNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::schedulePaulNotesForStep (int /*globalStep*/, int wrappedStep, double samplesPerStep,
                                                int sampleOffset, juce::MidiBuffer& midi)
{
    const GuitarHit& hit = guitarEngine.getStep (wrappedStep);
    if (! hit.active) return;

    int swingOff = guitarEngine.getSwingOffset (wrappedStep, samplesPerStep);
    int humanOff = hit.timeShift;
    int finalOff = juce::jlimit (0, samplesPerBlock - 1, sampleOffset + swingOff + humanOff);

    uint8 vel  = hit.velocity;
    int   note = juce::jlimit (0, 127, hit.midiNote);

    midi.addEvent (juce::MidiMessage::noteOn (paulMidiChannel, note, vel), finalOff);

    int durSamples = guitarEngine.calcNoteDuration (hit, samplesPerStep);
    int64 offAt    = midiSampleClock + finalOff + durSamples;
    paulPendingNoteOffs.add ({ paulMidiChannel, note, offAt });
}

void BridgeProcessor::sendPaulNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = paulPendingNoteOffs.size() - 1; i >= 0; --i)
    {
        auto& off = paulPendingNoteOffs.getReference (i);
        if (off.atSample <= midiSampleClock + sampleOffset)
        {
            int relSample = (int)juce::jlimit (0LL, (int64)(samplesPerBlock - 1),
                                               off.atSample - midiSampleClock);
            midi.addEvent (juce::MidiMessage::noteOff (off.channel, off.note), relSample);
            paulPendingNoteOffs.remove (i);
        }
    }
}

void BridgeProcessor::processPaulBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                                        const juce::AudioPlayHead::CurrentPositionInfo& pos, int numSamples)
{
    juce::ignoreUnused (buffer);

    syncPaulEngineFromAPVTS();
    guitarEngine.rebuildGridPreview();

    double bpm = pos.bpm > 1.0 ? pos.bpm : 120.0;
    paulLastBpm = bpm;

    double samplesPerBeat = sampleRate * 60.0 / bpm;
    double samplesPerStep = samplesPerBeat / 4.0;

    double ppqStart = pos.ppqPosition;
    double ppqEnd   = ppqStart + (double) numSamples / samplesPerBeat;

    constexpr double ppqPerStep = 0.25;

    int stepStart = (int)std::floor (ppqStart / ppqPerStep);
    int stepEnd   = (int)std::floor (ppqEnd   / ppqPerStep);

    int ls, le;
    getPaulLoopBounds (ls, le);
    const int ls0 = ls - 1;
    const int le0 = le - 1;
    const int loopLen = le0 - ls0 + 1;

    for (int globalStep = stepStart; globalStep <= stepEnd; ++globalStep)
    {
        double stepPPQ   = globalStep * ppqPerStep;
        if (stepPPQ < ppqStart || stepPPQ >= ppqEnd) continue;

        int sampleOffset = (int)((stepPPQ - ppqStart) * samplesPerBeat);
        sampleOffset     = juce::jlimit (0, juce::jmax (0, numSamples - 1), sampleOffset);

        const int mod = loopLen > 0 ? ((globalStep - ls0) % loopLen + loopLen) % loopLen : 0;
        int wrappedStep = ls0 + mod;

        const int v = (int)std::floor ((double) globalStep * paulTickerRate);
        const int visualWrapped = loopLen > 0
            ? (ls0 + ((v - ls0) % loopLen + loopLen) % loopLen)
            : ls0;

        int currRel = (globalStep - ls0) % loopLen;
        if (currRel < 0) currRel += loopLen;
        const bool isFirstStepOfLoop = (currRel == 0) && (globalStep != paulLastProcessedStep);

        if (isFirstStepOfLoop && globalStep != paulLastProcessedStep)
        {
            if (paulFillQueued)
            {
                const int fillStart = juce::jlimit (ls0, le0, paulFillFromStep);
                guitarEngine.generateFill (fillStart);
                paulFillQueued = false;
            }
            else if (guitarEngine.shouldTriggerFill())
            {
                guitarEngine.generateFill (juce::jmin (12, le0));
            }
        }

        paulCurrentStep.store (wrappedStep);
        paulCurrentVisualStep.store (visualWrapped);
        paulLastProcessedStep = globalStep;

        schedulePaulNotesForStep (globalStep, wrappedStep, samplesPerStep, sampleOffset, midiMessages);
    }

    sendPaulNoteOffs (midiMessages, numSamples > 0 ? numSamples - 1 : 0);
}

void BridgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    const auto mainEngineOn = [this] (const char* paramId) -> bool
    {
        if (auto* v = apvtsMain.getRawParameterValue (paramId))
            return v->load() > 0.5f;
        return true;
    };

    const bool animalOn = mainEngineOn ("animalOn");
    const bool bootsyOn = mainEngineOn ("bootsyOn");
    const bool stevieOn = mainEngineOn ("stevieOn");
    const bool paulOn   = mainEngineOn ("paulOn");

    if (! animalOn)
    {
        flushPendingNoteOffs (animalPendingNoteOffs, midiMessages);
        animalCurrentStep.store (-1);
        animalCurrentVisualStep.store (-1);
    }
    if (! bootsyOn)
    {
        flushPendingNoteOffs (bootsyPendingNoteOffs, midiMessages);
        bootsyCurrentStep.store (-1);
        bootsyCurrentVisualStep.store (-1);
    }
    if (! stevieOn)
    {
        flushPendingNoteOffs (steviePendingNoteOffs, midiMessages);
        stevieCurrentStep.store (-1);
        stevieCurrentVisualStep.store (-1);
    }
    if (! paulOn)
    {
        flushPendingNoteOffs (paulPendingNoteOffs, midiMessages);
        paulCurrentStep.store (-1);
        paulCurrentVisualStep.store (-1);
    }

    auto* playhead = getPlayHead();
    juce::AudioPlayHead::CurrentPositionInfo pos {};
    const bool transportRunning = playhead != nullptr
                                  && playhead->getCurrentPosition (pos)
                                  && pos.isPlaying;

    if (! transportRunning)
    {
        flushPendingNoteOffs (animalPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (bootsyPendingNoteOffs, midiMessages);
        flushPendingNoteOffs (steviePendingNoteOffs, midiMessages);
        flushPendingNoteOffs (paulPendingNoteOffs, midiMessages);
        animalCurrentStep.store (-1);
        animalCurrentVisualStep.store (-1);
        bootsyCurrentStep.store (-1);
        bootsyCurrentVisualStep.store (-1);
        stevieCurrentStep.store (-1);
        stevieCurrentVisualStep.store (-1);
        paulCurrentStep.store (-1);
        paulCurrentVisualStep.store (-1);
        midiSampleClock += numSamples;
        return;
    }

    if (animalOn)
        processAnimalBlock (buffer, midiMessages, pos, numSamples);
    if (bootsyOn)
        processBootsyBlock (buffer, midiMessages, pos, numSamples);
    if (stevieOn)
        processStevieBlock (buffer, midiMessages, pos, numSamples);
    if (paulOn)
        processPaulBlock (buffer, midiMessages, pos, numSamples);

    midiSampleClock += numSamples;
}

void BridgeProcessor::getStateInformation (juce::MemoryBlock& data)
{
    juce::XmlElement root ("BridgeState");
    root.setAttribute ("bridgeVersion", 2);
    if (auto xmlM = apvtsMain.copyState().createXml())
        root.addChildElement (xmlM.release());
    if (auto xmlA = apvtsAnimal.copyState().createXml())
        root.addChildElement (xmlA.release());
    if (auto xmlB = apvtsBootsy.copyState().createXml())
        root.addChildElement (xmlB.release());
    if (auto xmlS = apvtsStevie.copyState().createXml())
        root.addChildElement (xmlS.release());
    if (auto xmlP = apvtsPaul.copyState().createXml())
        root.addChildElement (xmlP.release());
    root.setAttribute ("activeTab", activeTab.load());
    copyXmlToBinary (root, data);
}

void BridgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("BridgeState"))
        {
            const int bridgeVersion = xml->getIntAttribute ("bridgeVersion", 1);
            int tab = xml->getIntAttribute ("activeTab", 0);
            if (bridgeVersion < 2)
                tab = (tab == 0) ? 1 : 2; // legacy: 0 animal, 1 bootsy
            else
                tab = juce::jlimit (0, 4, tab);
            activeTab.store (tab);

            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName (apvtsMain.state.getType()))
                    apvtsMain.replaceState (juce::ValueTree::fromXml (*child));
                else if (child->hasTagName (apvtsAnimal.state.getType()))
                    apvtsAnimal.replaceState (juce::ValueTree::fromXml (*child));
                else if (child->hasTagName (apvtsBootsy.state.getType()))
                    apvtsBootsy.replaceState (juce::ValueTree::fromXml (*child));
                else if (child->hasTagName (apvtsStevie.state.getType()))
                    apvtsStevie.replaceState (juce::ValueTree::fromXml (*child));
                else if (child->hasTagName (apvtsPaul.state.getType()))
                    apvtsPaul.replaceState (juce::ValueTree::fromXml (*child));
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BridgeProcessor();
}
