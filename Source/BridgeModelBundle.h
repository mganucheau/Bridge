#pragma once

#include <JuceHeader.h>

class BridgeMLManager;

/** Loads / installs signed .bridgemodels bundles (zip + manifest + per-file sha256). */
struct BridgeModelBundle
{
    /** Load from a directory of loose models (adds search path before app support) or install from .bridgemodels. */
    static juce::Result load (const juce::File& bundleOrDir, BridgeMLManager& mgr);

    /** Verify sha256 for every payload file, then extract to user app data models dir and reload ML. */
    static juce::Result installAndLoad (const juce::File& bundleFile, BridgeMLManager& mgr);

    /** Manifest "version" from the last successful load/install, or from disk if models are only from app support. */
    static juce::String getLoadedVersion();

    /** ~/Library/Application Support/Bridge/models (macOS) or %APPDATA%\\Bridge\\models (Windows). */
    static juce::File getModelsDirectory();

    /** Reads manifest.json "version" from getModelsDirectory(), if present. */
    static juce::String readInstalledManifestVersion();

    /** After ML reload, refresh cached version from disk (embedded/sidecar leaves manifest absent → empty). */
    static void refreshLoadedVersionFromDisk();

private:
    static juce::CriticalSection versionLock;
    static juce::String loadedVersionCache;
    static juce::File lastExtraModelsDir;

    static juce::Result verifyAndExtractToAppSupport (const juce::File& bundleFile);
    static juce::String readManifestVersionInDir (const juce::File& dir);
};
