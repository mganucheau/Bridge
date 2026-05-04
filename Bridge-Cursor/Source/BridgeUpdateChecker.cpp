#include "BridgeUpdateChecker.h"
#include "BridgeModelBundle.h"

#ifndef BRIDGE_UPDATE_MANIFEST_URL
 #define BRIDGE_UPDATE_MANIFEST_URL "https://updates.bridgeapp.io/models/manifest.json"
#endif

#ifndef BRIDGE_APP_VERSION
 #define BRIDGE_APP_VERSION 9
#endif

namespace
{
static int bundleTagRank (juce::String s)
{
    s = s.trim().toLowerCase();
    if (s.startsWithChar ('v'))
        s = s.substring (1);
    return s.getIntValue();
}
} // namespace

BridgeUpdateChecker::BridgeUpdateChecker()
    : juce::Thread ("BridgeUpdateChecker")
{
}

BridgeUpdateChecker::~BridgeUpdateChecker()
{
    stopThread (8000);
}

void BridgeUpdateChecker::checkForModelUpdates (std::function<void (UpdateInfo)> callback)
{
    {
        const juce::ScopedLock sl (cbLock);
        pendingCb = std::move (callback);
    }

    if (isThreadRunning())
        return;

    startThread (juce::Thread::Priority::background);
}

void BridgeUpdateChecker::run()
{
    UpdateInfo info;

    juce::URL url (BRIDGE_UPDATE_MANIFEST_URL);
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (8000)
                    .withNumRedirectsToFollow (3);
    std::unique_ptr<juce::InputStream> in (url.createInputStream (std::move (opts)));

    juce::String body;
    if (in != nullptr)
        body = in->readEntireStreamAsString();

    if (body.isNotEmpty())
    {
        const auto parsed = juce::JSON::parse (body);
        if (auto* o = parsed.getDynamicObject())
        {
            info.latestModelVersion = o->getProperty ("latest").toString();
            info.downloadUrl        = o->getProperty ("url").toString();
            const int minBv         = (int) o->getProperty ("min_bridge_version");

            if (minBv > 0 && minBv > BRIDGE_APP_VERSION)
            {
                info.isNewerThanInstalled = false;
            }
            else if (info.latestModelVersion.isNotEmpty() && info.downloadUrl.isNotEmpty())
            {
                juce::String installed = BridgeModelBundle::getLoadedVersion();
                if (installed.isEmpty())
                    installed = BridgeModelBundle::readInstalledManifestVersion();
                if (installed.isEmpty())
                    installed = "v0";

                const int remote = bundleTagRank (info.latestModelVersion);
                const int local  = bundleTagRank (installed);
                info.isNewerThanInstalled = remote > local;
            }
        }
    }

    std::function<void (UpdateInfo)> cb;
    {
        const juce::ScopedLock sl (cbLock);
        cb = std::move (pendingCb);
        pendingCb = nullptr;
    }

    if (cb != nullptr)
        juce::MessageManager::callAsync ([cb, info] { cb (info); });
}
