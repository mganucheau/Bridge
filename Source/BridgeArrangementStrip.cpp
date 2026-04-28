#include "BridgeArrangementStrip.h"
#include "BridgeProcessor.h"
#include "BridgeLookAndFeel.h"

namespace
{

class SectionBtn final : public juce::TextButton
{
public:
    SectionBtn (BridgeProcessor& processor, int secIndex, BridgeArrangementStrip& ownerStrip)
        : proc (processor), section (secIndex), owner (ownerStrip)
    {
        static const char* const names[5] = { "INTRO", "VERSE", "CHORUS", "BRIDGE", "OUTRO" };
        setButtonText (names[secIndex]);
        setRadioGroupId (199301);
        setClickingTogglesState (true);

        onClick = [this]
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("arrangement")))
                c->setValueNotifyingHost (c->convertTo0to1 ((float) section));
        };
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        const auto accent = bridge::colors::accentLeader();
        const auto dim    = bridge::colors::textDim();

        if (on)
        {
            g.setColour (accent.withAlpha (0.26f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (accent.withAlpha (0.75f));
        }
        else
        {
            g.setColour (dim.withAlpha (0.42f));
        }

        g.drawRoundedRectangle (r, 4.0f, 1.1f);

        g.setColour (on ? bridge::colors::textPrimary() : dim);
        g.setFont (getLookAndFeel().getTextButtonFont (*this, getHeight()));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Set from current");
            m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (&owner),
                             [this] (int r)
                             {
                                 if (r == 1)
                                     proc.captureArrangementTargetsForSection (section);
                                 owner.repaint();
                             });
            return;
        }

        juce::TextButton::mouseDown (e);
    }

private:
    BridgeProcessor& proc;
    int section;
    BridgeArrangementStrip& owner;
};

} // namespace

BridgeArrangementStrip::BridgeArrangementStrip (BridgeProcessor& p)
    : proc (p)
{
    for (int i = 0; i < 5; ++i)
    {
        auto b = std::make_unique<SectionBtn> (proc, i, *this);
        addAndMakeVisible (*b);
        sectionBtns[(size_t) i] = std::move (b);
    }

    proc.apvtsMain.addParameterListener ("arrangement", this);
    syncFromApvts();
}

BridgeArrangementStrip::~BridgeArrangementStrip()
{
    stopTimer();
    proc.apvtsMain.removeParameterListener ("arrangement", this);
}

void BridgeArrangementStrip::resized()
{
    auto area = getLocalBounds().reduced (10, 5);
    const int gap = 3;
    const int btnW = (area.getWidth() - gap * 4) / 5;
    int x = area.getX();
    for (int i = 0; i < 5; ++i)
    {
        if (sectionBtns[(size_t) i] != nullptr)
            sectionBtns[(size_t) i]->setBounds (x, area.getY(), btnW, area.getHeight());
        x += btnW + gap;
    }
}

void BridgeArrangementStrip::paintOverChildren (juce::Graphics& g)
{
    const float p = proc.getArrangementTransitionProgress();
    if (p < 0.0f)
        return;

    auto bar = getLocalBounds().removeFromBottom (3).reduced (10, 0);
    if (bar.isEmpty())
        return;

    g.setColour (bridge::colors::textDim().withAlpha (0.35f));
    g.fillRoundedRectangle (bar.toFloat(), 1.0f);

    const int fillW = juce::roundToInt ((float) bar.getWidth() * juce::jlimit (0.0f, 1.0f, p));
    auto fill = bar.withWidth (fillW);
    g.setColour (bridge::colors::accentLeader().withAlpha (0.88f));
    g.fillRoundedRectangle (fill.toFloat(), 1.0f);
}

void BridgeArrangementStrip::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == "arrangement")
    {
        syncFromApvts();
        if (proc.getArrangementTransitionProgress() >= 0.0f)
            startTimerHz (30);
        repaint();
    }
}

void BridgeArrangementStrip::timerCallback()
{
    if (proc.getArrangementTransitionProgress() >= 0.0f)
        repaint();
    else
        stopTimer();
}

void BridgeArrangementStrip::syncFromApvts()
{
    int idx = 1;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvtsMain.getParameter ("arrangement")))
        idx = c->getIndex();

    idx = juce::jlimit (0, 4, idx);

    for (int i = 0; i < 5; ++i)
    {
        if (sectionBtns[(size_t) i] == nullptr)
            continue;

        sectionBtns[(size_t) i]->setToggleState (i == idx, juce::dontSendNotification);
    }
}
