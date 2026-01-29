
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

/**
 * Neural Amp Modeler Effect
 *
 * Uses RTNeural for neural network inference to simulate guitar amplifiers.
 *
 * Architecture:
 * - VST: Uses XSIMD backend (SIMD optimized for x86 AVX/SSE or ARM NEON)
 * - Firmware: Uses STL backend (pure C++, no SIMD - Cortex-M7 lacks NEON)
 *
 * Model Types Supported:
 * - GRU-9 (firmware - optimized for multi-effect CPU budget)
 * - Additional sizes for VST runtime loading
 *
 * Model Format: RTNeural JSON (VST) or embedded constexpr (firmware)
 *
 * Usage:
 * - For firmware: Models are baked into flash at compile time via model_registry.h
 * - For VST: Models can be loaded from JSON files at runtime OR from embedded data
 *
 * Based on Mars project approach: GRU-9 with residual connection for
 * optimal quality-to-CPU ratio in multi-effect context.
 */

// Platform-specific neural network backend
#if defined(DAISY_SEED_BUILD)
// Firmware: Custom GRU-9 in ITCMRAM (no RTNeural dependency)
#include "effects/custom_gru9.h"
#else
// VST/Desktop: Use RTNeural with XSIMD backend
#define RTNEURAL_USE_XSIMD 1
#if __has_include(<RTNeural/RTNeural.h>)
#define HAS_RTNEURAL 1
#include <RTNeural/RTNeural.h>
#else
#define HAS_RTNEURAL 0
#endif
#endif

// Include shared embedded model registry for both firmware and VST
#include "embedded/model_registry.h"

#if !defined(DAISY_SEED_BUILD) && HAS_RTNEURAL
namespace NeuralAmpModels
{
    // VST: GRU with standard math (SIMD handles the heavy lifting)
    using GRU_9_1 = RTNeural::ModelT<float, 1, 1,
                                     RTNeural::GRULayerT<float, 1, 9>,
                                     RTNeural::DenseT<float, 9, 1>>;

    // Additional model types for VST
    using GRU_8_1 = RTNeural::ModelT<float, 1, 1,
                                     RTNeural::GRULayerT<float, 1, 8>,
                                     RTNeural::DenseT<float, 8, 1>>;

    using GRU_12_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 12>,
                                      RTNeural::DenseT<float, 12, 1>>;

    using GRU_16_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 16>,
                                      RTNeural::DenseT<float, 16, 1>>;

    using GRU_20_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 20>,
                                      RTNeural::DenseT<float, 20, 1>>;

    using GRU_32_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 32>,
                                      RTNeural::DenseT<float, 32, 1>>;

    using GRU_40_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 40>,
                                      RTNeural::DenseT<float, 40, 1>>;

    using LSTM_12_1 = RTNeural::ModelT<float, 1, 1,
                                       RTNeural::LSTMLayerT<float, 1, 12>,
                                       RTNeural::DenseT<float, 12, 1>>;

    using LSTM_16_1 = RTNeural::ModelT<float, 1, 1,
                                       RTNeural::LSTMLayerT<float, 1, 16>,
                                       RTNeural::DenseT<float, 16, 1>>;
}
#endif

/**
 * NeuralAmpEffect - Neural network-based amp simulator
 *
 * This effect uses a pre-trained neural network to simulate the nonlinear
 * characteristics of a guitar amplifier. The model processes audio sample-by-sample.
 *
 * Parameters:
 *   0: Model - Enum selection of embedded amp models
 *   1: Input - Input gain/drive
 *   2: Output - Output level
 *   3: Bass - Low frequency EQ
 *   4: Mid - Mid frequency EQ
 *   5: Treble - High frequency EQ
 */
struct NeuralAmpEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::NeuralAmp::TypeId;

    // Parameters
    uint8_t modelIndex_ = 0;  // id0: Model selection (enum)
    float inputGain_ = 0.5f;  // id1: Input gain/drive (0-1)
    float outputGain_ = 0.5f; // id2: Output level (0-1)
    float bass_ = 0.5f;       // id3: Bass EQ (post-model)
    float mid_ = 0.5f;        // id4: Mid EQ (post-model)
    float treble_ = 0.5f;     // id5: Treble EQ (post-model)

    // Internal state
    float sampleRate_ = 48000.0f;
    bool modelLoaded_ = false;
    float levelAdjust_ = 1.0f; // Per-model output level compensation

    // Loaded model info
    std::string modelName_ = "No Model";
    std::string modelPath_;

    // 3-band EQ using biquad filters
    // Low shelf (~200Hz), Mid peak (~800Hz), High shelf (~3kHz)
    struct BiquadState
    {
        float x1 = 0, x2 = 0; // Input history
        float y1 = 0, y2 = 0; // Output history
    };

    struct BiquadCoeffs
    {
        float b0 = 1, b1 = 0, b2 = 0;
        float a1 = 0, a2 = 0; // a0 normalized to 1
    };

    BiquadState bassState_, midState_, trebleState_;
    BiquadCoeffs bassCoeffs_, midCoeffs_, trebleCoeffs_;
    bool eqNeedsUpdate_ = true;

#if defined(DAISY_SEED_BUILD)
    CustomGRU9 model_;
#elif HAS_RTNEURAL
    NeuralAmpModels::GRU_9_1 model_;
#endif

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::NeuralAmp::kMeta; }

    void Init(float sampleRate) override
    {
        sampleRate_ = sampleRate;
        bassState_ = midState_ = trebleState_ = {};
        eqNeedsUpdate_ = true;

#if defined(DAISY_SEED_BUILD) || HAS_RTNEURAL
        model_.reset();
        // Load default embedded model
        LoadEmbeddedModel(0);
#endif
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            // Model selection (enum parameter)
            {
                uint8_t newIndex = static_cast<uint8_t>(v * 127.0f + 0.5f);
                // Clamp to valid range
                if (newIndex >= EmbeddedModels::kNumModels)
                    newIndex = EmbeddedModels::kNumModels - 1;
                if (newIndex != modelIndex_)
                {
                    modelIndex_ = newIndex;
                    LoadEmbeddedModel(newIndex);
                }
            }
            break;
        case 1:
            inputGain_ = v;
            break;
        case 2:
            outputGain_ = v;
            break;
        case 3:
            bass_ = v;
            eqNeedsUpdate_ = true;
            break;
        case 4:
            mid_ = v;
            eqNeedsUpdate_ = true;
            break;
        case 5:
            treble_ = v;
            eqNeedsUpdate_ = true;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 6)
            return 0;
        // Enum params: scale index to 0-127 range to match SetParam expectation
        out[0] = {0, modelIndex_}; // Enum: direct index value (0-7 for 8 models)
        out[1] = {1, (uint8_t)(inputGain_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(outputGain_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(bass_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(mid_ * 127.0f + 0.5f)};
        out[5] = {5, (uint8_t)(treble_ * 127.0f + 0.5f)};
        return 6;
    }

    // Get the name of the currently loaded model
    const std::string &GetModelName() const { return modelName_; }

    // Get the path to the currently loaded model
    const std::string &GetModelPath() const { return modelPath_; }

    // Check if a model is loaded and ready
    bool IsModelLoaded() const { return modelLoaded_; }

    /**
     * Load embedded model by index from model registry
     *
     * GRU-9 weight loading:
     *   setWVals: input-to-hidden weights [1 × 3*hidden]
     *   setUVals: hidden-to-hidden weights [hidden × 3*hidden]
     *   setBVals: biases [2 × 3*hidden]
     *
     * @param index Index into EmbeddedModels::kModelRegistry
     * @return true if model was loaded successfully
     */
    bool LoadEmbeddedModel(int index)
    {
        const auto *modelInfo = EmbeddedModels::GetModel(static_cast<size_t>(index));
        if (!modelInfo)
        {
            modelLoaded_ = false;
            modelName_ = "Invalid Model";
            return false;
        }

#if defined(DAISY_SEED_BUILD)
        // Firmware: load into custom GRU-9 (no RTNeural)
        model_.loadWeights(modelInfo->weightIH, modelInfo->weightHH,
                           modelInfo->bias, modelInfo->denseW,
                           modelInfo->denseB[0]);
#elif HAS_RTNEURAL
        constexpr int hiddenSize = 9;
        constexpr int gateSize = 3 * hiddenSize; // 27 for GRU (3 gates)

        // Load GRU layer weights
        std::vector<std::vector<float>> wVals(1, std::vector<float>(gateSize));
        for (int j = 0; j < gateSize; ++j)
            wVals[0][j] = modelInfo->weightIH[j];

        std::vector<std::vector<float>> uVals(hiddenSize, std::vector<float>(gateSize));
        for (int i = 0; i < hiddenSize; ++i)
            for (int j = 0; j < gateSize; ++j)
                uVals[i][j] = modelInfo->weightHH[i * gateSize + j];

        std::vector<std::vector<float>> bVals(2, std::vector<float>(gateSize));
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < gateSize; ++j)
                bVals[i][j] = modelInfo->bias[i * gateSize + j];

        // Load Dense layer weights
        std::vector<std::vector<float>> denseW(1, std::vector<float>(hiddenSize));
        for (int j = 0; j < hiddenSize; ++j)
            denseW[0][j] = modelInfo->denseW[j];
        float denseB[1] = {modelInfo->denseB[0]};

        auto &gruLayer = model_.template get<0>();
        gruLayer.setWVals(wVals);
        gruLayer.setUVals(uVals);
        gruLayer.setBVals(bVals);

        auto &denseLayer = model_.template get<1>();
        denseLayer.setWeights(denseW);
        denseLayer.setBias(denseB);

        model_.reset();
#else
        (void)index;
        return false;
#endif

        modelLoaded_ = true;
        modelName_ = modelInfo->name;
        modelIndex_ = static_cast<uint8_t>(index);
        levelAdjust_ = modelInfo->levelAdjust;

        return true;
    }

    // Simple utility functions
    static inline float fclamp(float x, float lo, float hi)
    {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static inline float dBToLinear(float dB)
    {
        return std::pow(10.0f, dB / 20.0f);
    }

    // Calculate low shelf biquad coefficients
    // freq: center frequency, gainDb: boost/cut in dB, Q: quality factor
    void calcLowShelf(BiquadCoeffs &c, float freq, float gainDb, float Q = 0.707f)
    {
        float A = dBToLinear(gainDb / 2.0f); // sqrt of linear gain
        float w0 = 2.0f * 3.14159265f * freq / sampleRate_;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);
        float sqrtA = std::sqrt(A);

        float a0 = (A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha;
        c.b0 = (A * ((A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha)) / a0;
        c.b1 = (2 * A * ((A - 1) - (A + 1) * cosw0)) / a0;
        c.b2 = (A * ((A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha)) / a0;
        c.a1 = (-2 * ((A - 1) + (A + 1) * cosw0)) / a0;
        c.a2 = ((A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha) / a0;
    }

    // Calculate high shelf biquad coefficients
    void calcHighShelf(BiquadCoeffs &c, float freq, float gainDb, float Q = 0.707f)
    {
        float A = dBToLinear(gainDb / 2.0f);
        float w0 = 2.0f * 3.14159265f * freq / sampleRate_;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);
        float sqrtA = std::sqrt(A);

        float a0 = (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
        c.b0 = (A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha)) / a0;
        c.b1 = (-2 * A * ((A - 1) + (A + 1) * cosw0)) / a0;
        c.b2 = (A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha)) / a0;
        c.a1 = (2 * ((A - 1) - (A + 1) * cosw0)) / a0;
        c.a2 = ((A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha) / a0;
    }

    // Calculate peaking EQ biquad coefficients
    void calcPeakingEQ(BiquadCoeffs &c, float freq, float gainDb, float Q = 1.0f)
    {
        float A = dBToLinear(gainDb / 2.0f);
        float w0 = 2.0f * 3.14159265f * freq / sampleRate_;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        float a0 = 1 + alpha / A;
        c.b0 = (1 + alpha * A) / a0;
        c.b1 = (-2 * cosw0) / a0;
        c.b2 = (1 - alpha * A) / a0;
        c.a1 = (-2 * cosw0) / a0;
        c.a2 = (1 - alpha / A) / a0;
    }

    // Process a sample through a biquad filter
    float processBiquad(BiquadState &s, const BiquadCoeffs &c, float x)
    {
        float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        return y;
    }

    // Update EQ coefficients when parameters change
    void updateEQCoeffs()
    {
        // Bass: Low shelf at 200Hz, ±12dB range
        float bassDb = (bass_ - 0.5f) * 24.0f; // 0→-12dB, 0.5→0dB, 1→+12dB
        calcLowShelf(bassCoeffs_, 200.0f, bassDb, 0.7f);

        // Mid: Peaking at 800Hz, ±12dB range, Q=1.0 for moderate width
        float midDb = (mid_ - 0.5f) * 24.0f;
        calcPeakingEQ(midCoeffs_, 800.0f, midDb, 1.0f);

        // Treble: High shelf at 3kHz, ±12dB range
        float trebleDb = (treble_ - 0.5f) * 24.0f;
        calcHighShelf(trebleCoeffs_, 3000.0f, trebleDb, 0.7f);

        eqNeedsUpdate_ = false;
    }

    // ITCMRAM-placed per-sample helper (firmware only, defined in neural_amp_itcmram.cpp)
#if defined(DAISY_SEED_BUILD)
    void ProcessSampleITCMRAM(float &l, float &r);
#endif

    void ProcessStereo(float &l, float &r) override
    {
        // Update EQ if parameters changed
        if (eqNeedsUpdate_)
            updateEQCoeffs();

#if defined(DAISY_SEED_BUILD)
        // Firmware: entire per-sample chain runs from ITCMRAM
        ProcessSampleITCMRAM(l, r);
#elif HAS_RTNEURAL
        // Convert to mono for neural network (amp models are typically mono)
        float mono = 0.5f * (l + r);

        // Apply input gain (-20dB to +20dB range)
        float inGain = FastMath::fastDbToLin((inputGain_ - 0.5f) * 40.0f);
        mono *= inGain;

        if (modelLoaded_)
        {
            // RTNeural forward pass (VST/Desktop)
            // Residual connection: output = model(input) + input
            float input[1] = {mono};
            mono = model_.forward(input) + input[0];
            mono *= levelAdjust_;
        }
        else
        {
            mono = std::tanh(mono * 2.0f) * 0.7f;
        }

        // Apply 3-band EQ (only process if not at neutral)
        float output = mono;

        // Bass shelf
        if (FastMath::fabs(bass_ - 0.5f) > 0.01f)
            output = processBiquad(bassState_, bassCoeffs_, output);

        // Mid peak
        if (FastMath::fabs(mid_ - 0.5f) > 0.01f)
            output = processBiquad(midState_, midCoeffs_, output);

        // Treble shelf
        if (FastMath::fabs(treble_ - 0.5f) > 0.01f)
            output = processBiquad(trebleState_, trebleCoeffs_, output);

        // Apply output gain (-20dB to +20dB range)
        float outGain = FastMath::fastDbToLin((outputGain_ - 0.5f) * 40.0f);
        output *= outGain;

        // Soft limit to prevent clipping
        output = fclamp(output, -1.5f, 1.5f);

        // Output to stereo
        l = r = output;
#else
        // No neural backend: simple soft clipping as placeholder
        float mono = 0.5f * (l + r);
        float inGain = FastMath::fastDbToLin((inputGain_ - 0.5f) * 40.0f);
        mono *= inGain;
        mono = std::tanh(mono * 2.0f) * 0.7f;
        float outGain = FastMath::fastDbToLin((outputGain_ - 0.5f) * 40.0f);
        mono *= outGain;
        mono = fclamp(mono, -1.5f, 1.5f);
        l = r = mono;
#endif
    }

#if !defined(DAISY_SEED_BUILD) && HAS_RTNEURAL
    /**
     * Load model weights from JSON (VST runtime loading)
     */
    template <typename JsonType>
    bool LoadModelFromJson(const JsonType &json, const std::string &name = "", const std::string &path = "")
    {
        try
        {
            model_.parseJson(json, true);
            model_.reset();
            modelLoaded_ = true;
            levelAdjust_ = 1.0f;

            if (!name.empty())
            {
                modelName_ = name;
            }
            else
            {
                try
                {
                    if (json.contains("model_data") && json["model_data"].contains("model_name"))
                        modelName_ = json["model_data"]["model_name"].template get<std::string>();
                    else if (json.contains("name"))
                        modelName_ = json["name"].template get<std::string>();
                    else
                        modelName_ = "Custom Model";
                }
                catch (...)
                {
                    modelName_ = "Custom Model";
                }
            }

            modelPath_ = path;
            return true;
        }
        catch (...)
        {
            modelLoaded_ = false;
            modelName_ = "Load Failed";
            return false;
        }
    }
#endif

    void ClearModel()
    {
        model_.reset();
        modelLoaded_ = false;
        modelName_ = "No Model";
        modelPath_.clear();
        levelAdjust_ = 1.0f;
    }

    void ResetModelState()
    {
        model_.reset();
    }
};
