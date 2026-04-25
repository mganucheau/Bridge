#include <JuceHeader.h>
#include "BridgeProcessor.h"
#include "ml/BridgeMLManager.h"

struct MLModelSmokeTests final : public juce::UnitTest
{
    MLModelSmokeTests()
        : juce::UnitTest ("ML models: ONNX load + inference smoke", "Bridge/QA")
    {
    }

    void runTest() override
    {
        beginTest ("DrumHumanizer model is loaded (or test explains why not)");

        BridgeProcessor proc;
        auto* ml = proc.getMLManager();
        expect (ml != nullptr, "BridgeProcessor should always create BridgeMLManager");
        if (ml == nullptr)
            return;

        const bool has = ml->hasModel (BridgeMLManager::ModelType::DrumHumanizer);
        if (! has)
        {
            // Don't fail CI: developer machines may not have extracted models. Still log loudly.
            logMessage ("DrumHumanizer ONNX not loaded (smoke skipped). Expected in either: "
                        "BinaryData, build sidecar models/, extraModelsDir, or "
                        "~/Library/Application Support/Bridge/models. "
                        "If you see onnxruntime trying to open *.onnx.data, your ONNX export may "
                        "be using external data and the .data file isn't shipped.");
            return;
        }

        beginTest ("DrumHumanizer inference produces 32 outputs");

        std::array<float, 10> knobs {};
        knobs.fill (0.5f);
        std::array<float, 32> prior {};
        prior.fill (0.0f);
        std::array<float, 4> groove { 0.0f, 0.5f, 0.5f, 0.5f };

        const auto out = ml->generateDrums (knobs, prior, groove);
        expectEquals ((int) out.size(), 32);
        if (out.size() != 32)
            return;

        // Sanity: not all zeros / NaN.
        int nFinite = 0;
        double sum = 0.0;
        for (auto v : out)
        {
            if (std::isfinite (v))
            {
                ++nFinite;
                sum += v;
            }
        }
        expect (nFinite == 32, "All outputs should be finite");
        expect (sum > 0.001, "Model output should not be all-zero");
    }
};

static MLModelSmokeTests mlModelSmokeTests;

