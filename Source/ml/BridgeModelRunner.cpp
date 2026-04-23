#include "BridgeModelRunner.h"
#include <JuceHeader.h>

#if BRIDGE_ML_ENABLED
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <exception>
#endif

struct BridgeModelRunner::Impl
{
#if BRIDGE_ML_ENABLED
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "Bridge" };
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
    std::vector<std::string> inputNameStorage;
    std::vector<std::string> outputNameStorage;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;

    Impl()
    {
        sessionOptions.SetIntraOpNumThreads (1);
        sessionOptions.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_BASIC);
    }
#endif
};

BridgeModelRunner::BridgeModelRunner()
{
#if BRIDGE_ML_ENABLED
    impl = new Impl();
#endif
}

BridgeModelRunner::~BridgeModelRunner()
{
#if BRIDGE_ML_ENABLED
    delete impl;
    impl = nullptr;
#endif
    loaded = false;
}

BridgeModelRunner::BridgeModelRunner (BridgeModelRunner&& o) noexcept
    : impl (o.impl), loaded (o.loaded)
{
    o.impl = nullptr;
    o.loaded = false;
}

BridgeModelRunner& BridgeModelRunner::operator= (BridgeModelRunner&& o) noexcept
{
    if (this != &o)
    {
#if BRIDGE_ML_ENABLED
        delete impl;
#endif
        impl = o.impl;
        loaded = o.loaded;
        o.impl = nullptr;
        o.loaded = false;
    }
    return *this;
}

bool BridgeModelRunner::loadFromMemory (const void* data, size_t sizeBytes, const char* logName)
{
#if ! BRIDGE_ML_ENABLED
    juce::ignoreUnused (data, sizeBytes, logName);
    return false;
#else
    if (data == nullptr || sizeBytes == 0)
        return false;

    juce::File tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile ("bridge_ort_", ".onnx", false);
    if (! tmp.replaceWithData (data, sizeBytes))
    {
        DBG ("BridgeModelRunner: failed to write temp model: " << (logName != nullptr ? logName : "?"));
        return false;
    }

    const bool ok = loadFromFile (tmp.getFullPathName().toStdString());
    tmp.deleteFile();
    if (! ok)
        DBG ("BridgeModelRunner: loadFromMemory failed: " << (logName != nullptr ? logName : "?"));
    return ok;
#endif
}

bool BridgeModelRunner::loadFromFile (const std::string& path)
{
#if ! BRIDGE_ML_ENABLED
    juce::ignoreUnused (path);
    return false;
#else
    try
    {
        if (impl == nullptr)
            impl = new Impl();

        impl->session = std::make_unique<Ort::Session> (impl->env, path.c_str(), impl->sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;
        const size_t inCount = impl->session->GetInputCount();
        const size_t outCount = impl->session->GetOutputCount();
        impl->inputNameStorage.clear();
        impl->outputNameStorage.clear();
        impl->inputNames.clear();
        impl->outputNames.clear();
        impl->inputNameStorage.reserve (inCount);
        impl->outputNameStorage.reserve (outCount);

        for (size_t i = 0; i < inCount; ++i)
        {
            auto name = impl->session->GetInputNameAllocated (i, allocator);
            impl->inputNameStorage.emplace_back (name.get());
        }
        for (size_t i = 0; i < outCount; ++i)
        {
            auto name = impl->session->GetOutputNameAllocated (i, allocator);
            impl->outputNameStorage.emplace_back (name.get());
        }
        for (auto& s : impl->inputNameStorage)
            impl->inputNames.push_back (s.c_str());
        for (auto& s : impl->outputNameStorage)
            impl->outputNames.push_back (s.c_str());

        loaded = (impl->session != nullptr && ! impl->inputNames.empty() && ! impl->outputNames.empty());
        return loaded;
    }
    catch (const Ort::Exception& e)
    {
        DBG ("BridgeModelRunner::loadFromFile Ort::Exception: " << e.what());
        loaded = false;
        return false;
    }
    catch (...)
    {
        DBG ("BridgeModelRunner::loadFromFile: unknown exception");
        loaded = false;
        return false;
    }
#endif
}

std::vector<float> BridgeModelRunner::run (const std::vector<float>& input) const
{
#if ! BRIDGE_ML_ENABLED
    juce::ignoreUnused (input);
    return {};
#else
    if (! loaded || impl == nullptr || impl->session == nullptr || input.empty())
        return {};

    try
    {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        const std::vector<int64_t> inShape { 1, (int64_t) input.size() };
        Ort::Value inTensor = Ort::Value::CreateTensor<float> (
            mem, const_cast<float*> (input.data()), input.size(), inShape.data(), inShape.size());

        auto outputs = impl->session->Run (Ort::RunOptions { nullptr },
                                           impl->inputNames.data(), &inTensor, 1,
                                           impl->outputNames.data(), 1);

        if (outputs.empty())
            return {};

        float* outData = outputs[0].GetTensorMutableData<float>();
        const size_t n = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        return std::vector<float> (outData, outData + n);
    }
    catch (const Ort::Exception& e)
    {
        DBG ("BridgeModelRunner::run: " << e.what());
        return {};
    }
    catch (...)
    {
        return {};
    }
#endif
}
