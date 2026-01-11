
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

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
 * - LSTM-12 (both firmware and VST - standardized for embedded models)
 * - Additional sizes for VST runtime loading
 *
 * Model Format: AIDA-X / RTNeural JSON (VST) or embedded constexpr (firmware)
 *
 * Usage:
 * - For firmware: Models are baked into flash at compile time via model_registry.h
 * - For VST: Models can be loaded from JSON files at runtime OR from embedded data
 */

// Platform-specific RTNeural configuration
#if defined(DAISY_SEED_BUILD)
// Daisy Seed: Use STL backend (Cortex-M7 has FPU but no NEON)
#define RTNEURAL_USE_STL 1
#else
// VST/Desktop: Use XSIMD for SIMD optimization
#define RTNEURAL_USE_XSIMD 1
#endif

// Only include RTNeural if available (guarded for builds without it)
#if __has_include(<RTNeural/RTNeural.h>)
#define HAS_RTNEURAL 1
#include <RTNeural/RTNeural.h>
#else
#define HAS_RTNEURAL 0
#endif

// Include embedded model registry for firmware builds
#if defined(DAISY_SEED_BUILD)
#include "embedded/model_registry.h"
#endif

namespace NeuralAmpModels
{
    // Supported model architectures for compile-time instantiation
    // Format: Type_HiddenSize_InputSize

#if HAS_RTNEURAL
    // Small models suitable for Daisy Seed firmware
    using GRU_8_1 = RTNeural::ModelT<float, 1, 1,
                                     RTNeural::GRULayerT<float, 1, 8>,
                                     RTNeural::DenseT<float, 8, 1>>;

    using GRU_12_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 12>,
                                      RTNeural::DenseT<float, 12, 1>>;

    using GRU_16_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 16>,
                                      RTNeural::DenseT<float, 16, 1>>;

    // Medium models for VST
    using GRU_20_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 20>,
                                      RTNeural::DenseT<float, 20, 1>>;

    using GRU_32_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 32>,
                                      RTNeural::DenseT<float, 32, 1>>;

    using GRU_40_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::GRULayerT<float, 1, 40>,
                                      RTNeural::DenseT<float, 40, 1>>;

    // LSTM variants (higher quality, more CPU)
    using LSTM_8_1 = RTNeural::ModelT<float, 1, 1,
                                      RTNeural::LSTMLayerT<float, 1, 8>,
                                      RTNeural::DenseT<float, 8, 1>>;

    using LSTM_12_1 = RTNeural::ModelT<float, 1, 1,
                                       RTNeural::LSTMLayerT<float, 1, 12>,
                                       RTNeural::DenseT<float, 12, 1>>;

    using LSTM_16_1 = RTNeural::ModelT<float, 1, 1,
                                       RTNeural::LSTMLayerT<float, 1, 16>,
                                       RTNeural::DenseT<float, 16, 1>>;
#endif
}

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

#if HAS_RTNEURAL
// Use LSTM-12 which is compatible with AIDA-X models
// Standardized for embedded models on both firmware and VST
#if defined(DAISY_SEED_BUILD)
    NeuralAmpModels::LSTM_12_1 model_; // For firmware
#else
    NeuralAmpModels::LSTM_12_1 model_; // For VST - matches AIDA-X default
#endif
#endif

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::NeuralAmp::kMeta; }

    void Init(float sampleRate) override
    {
        sampleRate_ = sampleRate;
        bassState_ = midState_ = trebleState_ = {};
        eqNeedsUpdate_ = true;

#if HAS_RTNEURAL
        model_.reset();
        
#if defined(DAISY_SEED_BUILD)
        // Load default embedded model on firmware
        LoadEmbeddedModel(0);
#endif
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
#if defined(DAISY_SEED_BUILD)
                if (newIndex >= EmbeddedModels::kNumModels)
                    newIndex = EmbeddedModels::kNumModels - 1;
#else
                // For VST, limit to number of embedded models for now
                if (newIndex >= 5)
                    newIndex = 4;
#endif
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
        out[0] = {0, modelIndex_};  // Enum params use direct value
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
     * @param index Index into EmbeddedModels::kModelRegistry
     * @return true if model was loaded successfully
     */
    bool LoadEmbeddedModel(int index)
    {
#if HAS_RTNEURAL && defined(DAISY_SEED_BUILD)
        const auto* modelInfo = EmbeddedModels::GetModel(static_cast<size_t>(index));
        if (!modelInfo)
        {
            modelLoaded_ = false;
            modelName_ = "Invalid Model";
            return false;
        }
        
        // Load LSTM layer weights
        // RTNeural LSTM expects:
        //   setWVals: kernel weights [input_size][4*hidden_size]
        //   setUVals: recurrent weights [hidden_size][4*hidden_size]
        //   setBVals: biases [4*hidden_size]
        
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
        auto& lstmLayer = model_.template get<0>();
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
        auto& denseLayer = model_.template get<1>();
        denseLayer.setWeights(denseW);
        denseLayer.setBias(denseB);
        
        // Reset model state and mark as loaded
        model_.reset();
        modelLoaded_ = true;
        modelName_ = modelInfo->name;
        modelIndex_ = static_cast<uint8_t>(index);
        
        return true;
#else
        // For VST without embedded registry, just mark as loaded
        // (VST uses LoadModelFromJson for runtime loading)
        (void)index;
        return false;
#endif
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

    void ProcessStereo(float &l, float &r) override
    {
        // Update EQ if parameters changed
        if (eqNeedsUpdate_)
            updateEQCoeffs();

        // Convert to mono for neural network (amp models are typically mono)
        float mono = 0.5f * (l + r);

        // Apply input gain (-20dB to +20dB range)
        float inGain = dBToLinear((inputGain_ - 0.5f) * 40.0f);
        mono *= inGain;

#if HAS_RTNEURAL
        if (modelLoaded_)
        {
            // Process through neural network (sample-by-sample)
            float input[1] = {mono};
            mono = model_.forward(input);
        }
        else
        {
            // Fallback: simple soft clipping when no model loaded
            mono = std::tanh(mono * 2.0f) * 0.7f;
        }
#else
        // No RTNeural: simple soft clipping as placeholder
        mono = std::tanh(mono * 2.0f) * 0.7f;
#endif

        // Apply 3-band EQ (only process if not at neutral)
        float output = mono;

        // Bass shelf
        if (std::fabs(bass_ - 0.5f) > 0.01f)
            output = processBiquad(bassState_, bassCoeffs_, output);

        // Mid peak
        if (std::fabs(mid_ - 0.5f) > 0.01f)
            output = processBiquad(midState_, midCoeffs_, output);

        // Treble shelf
        if (std::fabs(treble_ - 0.5f) > 0.01f)
            output = processBiquad(trebleState_, trebleCoeffs_, output);

        // Apply output gain (-20dB to +20dB range)
        float outGain = dBToLinear((outputGain_ - 0.5f) * 40.0f);
        output *= outGain;

        // Soft limit to prevent clipping
        output = fclamp(output, -1.5f, 1.5f);

        // Output to stereo
        l = r = output;
    }

#if HAS_RTNEURAL
    /**
     * Load model weights from JSON (VST runtime loading)
     *
     * JSON format is AIDA-X compatible:
     * {
     *   "model_data": {
     *     "model_name": "...",
     *     ...
     *   },
     *   "state_dict": { ... }
     * }
     *
     * @param json The parsed JSON object
     * @param name Optional model name to use (if not found in JSON)
     * @param path Optional path to store for reference
     * @return true if model was loaded successfully
     */
    template <typename JsonType>
    bool LoadModelFromJson(const JsonType &json, const std::string &name = "", const std::string &path = "")
    {
        try
        {
            model_.parseJson(json, true);
            model_.reset();
            modelLoaded_ = true;

            // Try to extract model name from JSON metadata
            if (!name.empty())
            {
                modelName_ = name;
            }
            else
            {
                // Try to find name in AIDA-X format
                try
                {
                    if (json.contains("model_data") && json["model_data"].contains("model_name"))
                    {
                        modelName_ = json["model_data"]["model_name"].template get<std::string>();
                    }
                    else if (json.contains("name"))
                    {
                        modelName_ = json["name"].template get<std::string>();
                    }
                    else
                    {
                        modelName_ = "Custom Model";
                    }
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

    /**
     * Clear the loaded model
     */
    void ClearModel()
    {
        model_.reset();
        modelLoaded_ = false;
        modelName_ = "No Model";
        modelPath_.clear();
    }

    void ResetModelState()
    {
        model_.reset();
    }
#endif
};
