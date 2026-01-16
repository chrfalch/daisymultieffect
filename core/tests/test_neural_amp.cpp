/**
 * Neural Amp Model Verification Tests
 *
 * Tests to verify that manual weight loading (LoadEmbeddedModel style)
 * produces the same output as RTNeural's parseJson() loader.
 *
 * Uses Catch2 v3 (header-only).
 *
 * Compile and run manually:
 *   # Download Catch2 v3 header
 *   curl -L https://github.com/catchorg/Catch2/releases/download/v3.5.0/catch_amalgamated.hpp -o catch_amalgamated.hpp
 *   curl -L https://github.com/catchorg/Catch2/releases/download/v3.5.0/catch_amalgamated.cpp -o catch_amalgamated.cpp
 *
 *   # Compile (macOS with RTNeural via brew or local build)
 *   clang++ -std=c++17 -O2 \
 *       -I/path/to/RTNeural/include \
 *       -I../../ \
 *       catch_amalgamated.cpp \
 *       test_neural_amp.cpp \
 *       -o test_neural_amp
 *
 *   # Run
 *   ./test_neural_amp
 */

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

// RTNeural for model inference
#define RTNEURAL_USE_XSIMD 1
#include <RTNeural/RTNeural.h>

// Model registry for embedded weights
#include "../effects/embedded/model_registry.h"

#include <cmath>
#include <vector>
#include <iostream>

// LSTM-12 model type (same as NeuralAmpEffect)
using LSTM_12_1 = RTNeural::ModelT<float, 1, 1,
                                   RTNeural::LSTMLayerT<float, 1, 12>,
                                   RTNeural::DenseT<float, 12, 1>>;

/**
 * Load model weights manually, exactly as LoadEmbeddedModel does.
 */
void loadModelManually(LSTM_12_1 &model, const EmbeddedModels::ModelInfo *modelInfo)
{
    constexpr int hiddenSize = 12;
    constexpr int gateSize = 4 * hiddenSize; // 48 for LSTM

    // Convert flat arrays to vector<vector> for RTNeural API
    // Kernel: shape (1, 48) -> vector<vector>[1][48]
    std::vector<std::vector<float>> wVals(1, std::vector<float>(gateSize));
    for (int j = 0; j < gateSize; ++j)
        wVals[0][j] = modelInfo->kernel[j];

    // Recurrent: shape (12, 48) -> vector<vector>[12][48]
    std::vector<std::vector<float>> uVals(hiddenSize, std::vector<float>(gateSize));
    for (int i = 0; i < hiddenSize; ++i)
        for (int j = 0; j < gateSize; ++j)
            uVals[i][j] = modelInfo->recurrent[i * gateSize + j];

    // Biases: shape (48,) -> vector[48]
    std::vector<float> bVals(gateSize);
    for (int j = 0; j < gateSize; ++j)
        bVals[j] = modelInfo->bias[j];

    // Set LSTM weights (layer 0)
    auto &lstmLayer = model.template get<0>();
    lstmLayer.setWVals(wVals);
    lstmLayer.setUVals(uVals);
    lstmLayer.setBVals(bVals);

    // Load Dense layer weights
    // Dense: shape (12, 1) -> vector<vector>[1][12]
    std::vector<std::vector<float>> denseW(1, std::vector<float>(hiddenSize));
    for (int j = 0; j < hiddenSize; ++j)
        denseW[0][j] = modelInfo->denseW[j];

    // Dense bias: shape (1,)
    float denseB[1] = {modelInfo->denseB[0]};

    // Set Dense weights (layer 1)
    auto &denseLayer = model.template get<1>();
    denseLayer.setWeights(denseW);
    denseLayer.setBias(denseB);

    model.reset();
}

/**
 * Generate a simple test signal (sine wave + impulse).
 */
std::vector<float> generateTestSignal(int numSamples, float sampleRate = 48000.0f)
{
    std::vector<float> signal(numSamples);

    // Start with an impulse
    signal[0] = 1.0f;

    // Add a 440Hz sine wave
    for (int i = 0; i < numSamples; ++i)
    {
        float t = static_cast<float>(i) / sampleRate;
        signal[i] += 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * t);
    }

    return signal;
}

/**
 * Process signal through model and return output.
 */
std::vector<float> processSignal(LSTM_12_1 &model, const std::vector<float> &input)
{
    std::vector<float> output(input.size());

    model.reset();
    for (size_t i = 0; i < input.size(); ++i)
    {
        float in[1] = {input[i]};
        output[i] = model.forward(in);
    }

    return output;
}

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("Model registry has expected models", "[registry]")
{
    REQUIRE(EmbeddedModels::kNumModels >= 1);

    const auto *model0 = EmbeddedModels::GetModel(0);
    REQUIRE(model0 != nullptr);
    REQUIRE(model0->name != nullptr);
    REQUIRE(model0->hiddenSize == 12);

    INFO("Model 0: " << model0->name);
}

TEST_CASE("Weight array sizes are correct for LSTM-12", "[weights]")
{
    const auto *modelInfo = EmbeddedModels::GetModel(0);
    REQUIRE(modelInfo != nullptr);

    // LSTM-12 expected sizes
    REQUIRE(modelInfo->kernelSize == 48);     // 1 * 4 * 12
    REQUIRE(modelInfo->recurrentSize == 576); // 12 * 4 * 12
    REQUIRE(modelInfo->biasSize == 48);       // 4 * 12
    REQUIRE(modelInfo->denseWSize == 12);     // 12 * 1
    REQUIRE(modelInfo->denseBSize == 1);      // 1
}

TEST_CASE("Manual weight loading produces valid output", "[inference]")
{
    LSTM_12_1 model;
    const auto *modelInfo = EmbeddedModels::GetModel(0);
    REQUIRE(modelInfo != nullptr);

    loadModelManually(model, modelInfo);

    // Process a simple impulse
    float input[1] = {1.0f};
    float output = model.forward(input);

    // Output should be a finite number (not NaN or Inf)
    REQUIRE(std::isfinite(output));

    // Output magnitude should be reasonable (not exploding)
    REQUIRE(std::abs(output) < 100.0f);

    INFO("Impulse response first sample: " << output);
}

TEST_CASE("Model output matches Python reference (tw40_california_clean)", "[reference]")
{
    // This test verifies against known-good Python LSTM implementation output
    // for the tw40_california_clean_deerinkstudios model.
    //
    // Expected values from: python test_model_inference.py tw40_california_clean_deerinkstudios.json

    LSTM_12_1 model;
    const auto *modelInfo = EmbeddedModels::GetModel(0);
    REQUIRE(modelInfo != nullptr);

    // Verify we're testing the right model
    std::string modelName = modelInfo->name;
    bool isExpectedModel = (modelName.find("tw40_california_clean") != std::string::npos);
    if (!isExpectedModel)
    {
        WARN("Model 0 is not tw40_california_clean, skipping reference test");
        SKIP("Model 0 is " << modelName);
    }

    loadModelManually(model, modelInfo);

    // Test 1: Impulse response first sample
    // Expected from Python: 0.24149173
    float input[1] = {1.0f};
    float output = model.forward(input);
    INFO("Impulse response: got " << output << ", expected 0.24149173");
    REQUIRE(output == Catch::Approx(0.24149173f).margin(1e-4f));

    // Test 2: DC input after 10 samples
    // Expected from Python: 0.01089057
    model.reset();
    for (int i = 0; i < 10; ++i)
    {
        float dc_in[1] = {0.5f};
        output = model.forward(dc_in);
    }
    INFO("DC response after 10 samples: got " << output << ", expected 0.01089057");
    REQUIRE(output == Catch::Approx(0.01089057f).margin(1e-4f));

    // Test 3: Full impulse sequence
    // Expected from Python: [0.24149173, -0.19917969, -0.30230665, -0.17716669, -0.05524690]
    model.reset();
    float impulse_expected[] = {0.24149173f, -0.19917969f, -0.30230665f, -0.17716669f, -0.05524690f};
    float impulse_sequence[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 5; ++i)
    {
        float in[1] = {impulse_sequence[i]};
        output = model.forward(in);
        INFO("Impulse[" << i << "]: got " << output << ", expected " << impulse_expected[i]);
        REQUIRE(output == Catch::Approx(impulse_expected[i]).margin(1e-4f));
    }
}

TEST_CASE("Different models produce different outputs", "[inference]")
{
    if (EmbeddedModels::kNumModels < 2)
    {
        SKIP("Need at least 2 models to compare");
    }

    LSTM_12_1 model1, model2;
    const auto *info1 = EmbeddedModels::GetModel(0);
    const auto *info2 = EmbeddedModels::GetModel(1);

    REQUIRE(info1 != nullptr);
    REQUIRE(info2 != nullptr);

    loadModelManually(model1, info1);
    loadModelManually(model2, info2);

    auto testSignal = generateTestSignal(100);
    auto output1 = processSignal(model1, testSignal);
    auto output2 = processSignal(model2, testSignal);

    // Outputs should differ (models are different)
    float maxDiff = 0.0f;
    for (size_t i = 0; i < output1.size(); ++i)
    {
        float diff = std::abs(output1[i] - output2[i]);
        if (diff > maxDiff)
            maxDiff = diff;
    }

    INFO("Max difference between models: " << maxDiff);
    REQUIRE(maxDiff > 1e-6f); // Should have some difference
}

TEST_CASE("Model produces consistent output across runs", "[determinism]")
{
    LSTM_12_1 model;
    const auto *modelInfo = EmbeddedModels::GetModel(0);
    REQUIRE(modelInfo != nullptr);

    loadModelManually(model, modelInfo);

    auto testSignal = generateTestSignal(50);

    // First run
    auto output1 = processSignal(model, testSignal);

    // Reload and second run
    loadModelManually(model, modelInfo);
    auto output2 = processSignal(model, testSignal);

    // Outputs should be identical
    for (size_t i = 0; i < output1.size(); ++i)
    {
        REQUIRE(output1[i] == Catch::Approx(output2[i]).epsilon(1e-9));
    }
}

TEST_CASE("Model processes longer signal without exploding", "[stability]")
{
    LSTM_12_1 model;
    const auto *modelInfo = EmbeddedModels::GetModel(0);
    REQUIRE(modelInfo != nullptr);

    loadModelManually(model, modelInfo);

    // Process 1 second of audio at 48kHz
    auto testSignal = generateTestSignal(48000);
    auto output = processSignal(model, testSignal);

    // Check all outputs are finite
    for (size_t i = 0; i < output.size(); ++i)
    {
        REQUIRE(std::isfinite(output[i]));
    }

    // Check output doesn't explode
    float maxAbs = 0.0f;
    for (float v : output)
    {
        if (std::abs(v) > maxAbs)
            maxAbs = std::abs(v);
    }

    INFO("Max output magnitude over 1 second: " << maxAbs);
    REQUIRE(maxAbs < 10.0f); // Should stay bounded
}

// Optional: Compare with JSON loading if you have the JSON embedded
#ifdef TEST_JSON_COMPARISON
TEST_CASE("Manual loading matches parseJson loading", "[json_comparison]")
{
    // This test requires embedding the JSON as a string
    // or loading from file, which adds complexity.
    // For now, we rely on the Python verification script.
    SKIP("JSON comparison test not implemented - use verify_conversion.py");
}
#endif
