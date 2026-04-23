#include "BridgeModelBundle.h"
#include "ml/BridgeMLManager.h"
#include <juce_cryptography/juce_cryptography.h>

juce::CriticalSection BridgeModelBundle::versionLock;
juce::String BridgeModelBundle::loadedVersionCache;
juce::File BridgeModelBundle::lastExtraModelsDir;

namespace
{
static constexpr const char* kManifestName = "manifest.json";

static bool sha256HexMatches (const void* data, size_t numBytes, juce::StringRef expectedHex)
{
    return juce::SHA256 (data, numBytes).toHexString().equalsIgnoreCase (expectedHex);
}

static juce::var readJsonFile (const juce::File& f)
{
    if (! f.existsAsFile())
        return {};
    return juce::JSON::parse (f.loadFileAsString());
}
} // namespace

juce::File BridgeModelBundle::getModelsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Bridge")
        .getChildFile ("models");
}

juce::String BridgeModelBundle::readManifestVersionInDir (const juce::File& dir)
{
    const auto root = readJsonFile (dir.getChildFile (kManifestName));
    if (auto* o = root.getDynamicObject())
        return o->getProperty ("version").toString();
    return {};
}

juce::String BridgeModelBundle::readInstalledManifestVersion()
{
    return readManifestVersionInDir (getModelsDirectory());
}

void BridgeModelBundle::refreshLoadedVersionFromDisk()
{
    const juce::ScopedLock sl (versionLock);
    if (lastExtraModelsDir.isDirectory())
        loadedVersionCache = readManifestVersionInDir (lastExtraModelsDir);
    else
        loadedVersionCache = readInstalledManifestVersion();
}

juce::String BridgeModelBundle::getLoadedVersion()
{
    const juce::ScopedLock sl (versionLock);
    return loadedVersionCache;
}

juce::Result BridgeModelBundle::verifyAndExtractToAppSupport (const juce::File& bundleFile)
{
    if (! bundleFile.existsAsFile())
        return juce::Result::fail ("Bundle file not found.");

    juce::FileInputStream fin (bundleFile);
    if (! fin.openedOk())
        return juce::Result::fail ("Could not open bundle.");

    juce::ZipFile zip (&fin, false);
    juce::String manifestText;
    {
        const auto* ze = zip.getEntry (kManifestName);
        if (ze == nullptr)
            return juce::Result::fail ("Missing manifest in bundle.");

        const std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (*ze));
        if (in == nullptr)
            return juce::Result::fail ("Missing manifest in bundle.");
        manifestText = in->readEntireStreamAsString();
    }

    const auto manifestRoot = juce::JSON::parse (manifestText);
    auto* rootObj = manifestRoot.getDynamicObject();
    auto* filesObj = rootObj != nullptr ? rootObj->getProperty ("files").getDynamicObject() : nullptr;
    if (filesObj == nullptr)
        return juce::Result::fail ("Invalid manifest.");

    juce::Array<std::pair<juce::String, juce::MemoryBlock>> verified;

    for (const auto& nv : filesObj->getProperties())
    {
        const auto name = nv.name.toString();
        const auto expectHex = nv.value.toString();
        const auto* ze = zip.getEntry (name);
        if (ze == nullptr)
            return juce::Result::fail ("Missing file in bundle: " + name);

        const std::unique_ptr<juce::InputStream> zin (zip.createStreamForEntry (*ze));
        if (zin == nullptr)
            return juce::Result::fail ("Could not read: " + name);

        juce::MemoryBlock data;
        zin->readIntoMemoryBlock (data);
        if (! sha256HexMatches (data.getData(), data.getSize(), expectHex))
            return juce::Result::fail ("Checksum mismatch: " + name);

        verified.add ({ name, std::move (data) });
    }

    auto dest = getModelsDirectory();
    if (! dest.createDirectory())
        return juce::Result::fail ("Could not create models directory.");

    for (const auto& pair : verified)
    {
        const juce::File outF = dest.getChildFile (pair.first);
        if (! outF.replaceWithData (pair.second.getData(), pair.second.getSize()))
            return juce::Result::fail ("Could not write: " + pair.first);
    }

    const juce::File manOut = dest.getChildFile (kManifestName);
    if (! manOut.replaceWithText (manifestText, false, false, nullptr))
        return juce::Result::fail ("Could not write manifest.");

    return juce::Result::ok();
}

juce::Result BridgeModelBundle::installAndLoad (const juce::File& bundleFile, BridgeMLManager& mgr)
{
    const auto r = verifyAndExtractToAppSupport (bundleFile);
    if (! r.wasOk())
        return r;

    lastExtraModelsDir = juce::File();
    mgr.clearExtraModelsSearchDirectory();
    mgr.reloadAllModels();

    const juce::ScopedLock sl (versionLock);
    loadedVersionCache = readInstalledManifestVersion();
    return juce::Result::ok();
}

juce::Result BridgeModelBundle::load (const juce::File& bundleOrDir, BridgeMLManager& mgr)
{
    if (bundleOrDir.isDirectory())
    {
        lastExtraModelsDir = bundleOrDir;
        mgr.setExtraModelsSearchDirectory (bundleOrDir);
        mgr.reloadAllModels();
        const juce::ScopedLock sl (versionLock);
        loadedVersionCache = readManifestVersionInDir (bundleOrDir);
        return juce::Result::ok();
    }

    if (bundleOrDir.getFileExtension().equalsIgnoreCase ("bridgemodels"))
        return installAndLoad (bundleOrDir, mgr);

    return juce::Result::fail ("Unsupported bundle path.");
}
