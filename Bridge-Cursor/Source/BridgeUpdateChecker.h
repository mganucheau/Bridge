#pragma once

#include <JuceHeader.h>
#include <functional>

/** One-shot background GET of the public model manifest; silent on failure. */
class BridgeUpdateChecker : private juce::Thread
{
public:
    struct UpdateInfo
    {
        juce::String latestModelVersion;
        juce::String downloadUrl;
        bool isNewerThanInstalled = false;
    };

    BridgeUpdateChecker();
    ~BridgeUpdateChecker() override;

    /** Starts a background fetch; invokes callback on the message thread when finished (always). */
    void checkForModelUpdates (std::function<void (UpdateInfo)> callback);

    bool isUpdateCheckInProgress() const { return isThreadRunning(); }

private:
    void run() override;

    std::function<void (UpdateInfo)> pendingCb;
    juce::CriticalSection cbLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeUpdateChecker)
};
