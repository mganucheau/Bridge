#pragma once

#include <vector>
#include <string>
#include <cstddef>

/** Thin wrapper around ONNX Runtime. Safe when BRIDGE_ML_ENABLED=0 (stubs). */
class BridgeModelRunner
{
public:
    BridgeModelRunner();
    ~BridgeModelRunner();

    BridgeModelRunner (const BridgeModelRunner&) = delete;
    BridgeModelRunner& operator= (const BridgeModelRunner&) = delete;
    BridgeModelRunner (BridgeModelRunner&&) noexcept;
    BridgeModelRunner& operator= (BridgeModelRunner&&) noexcept;

    bool loadFromMemory (const void* data, size_t sizeBytes, const char* logName);
    bool loadFromFile (const std::string& path);

    std::vector<float> run (const std::vector<float>& input) const;

    bool isLoaded() const { return loaded; }

private:
    struct Impl;
    Impl* impl = nullptr;
    bool loaded = false;
};
