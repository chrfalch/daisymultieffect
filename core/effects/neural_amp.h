
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>
#include <cstdint>
#include <string>

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
 * - GRU-8, GRU-12, GRU-16 (firmware - small, fast)
 * - GRU-8 to GRU-40 (VST - more variety)
 * - LSTM-8 to LSTM-32 (VST only - higher quality, more CPU)
 *
 * Model Format: AIDA-X / RTNeural JSON
 *
 * Usage:
 * - For firmware: Models are baked into flash at compile time
 * - For VST: Models can be loaded from JSON files at runtime
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
 */
struct NeuralAmpEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::NeuralAmp::TypeId;

    // Parameters
    float inputGain_ = 0.5f;  // id0: Input gain/drive (0-1)
    float outputGain_ = 0.5f; // id1: Output level (0-1)
    float bass_ = 0.5f;       // id2: Bass EQ (post-model)
    float mid_ = 0.5f;        // id3: Mid EQ (post-model)
    float treble_ = 0.5f;     // id4: Treble EQ (post-model)

    // Internal state
    float sampleRate_ = 48000.0f;
    bool modelLoaded_ = false;

    // Loaded model info
    std::string modelName_ = "No Model";
    std::string modelPath_;

    // Simple tone stack state (post-model EQ)
    float lpfState_ = 0.0f;
    float hpfState_ = 0.0f;
    float bpfState1_ = 0.0f;
    float bpfState2_ = 0.0f;

#if HAS_RTNEURAL
// Use LSTM-12 which is compatible with AIDA-X models
// Can be changed to other model types based on the loaded model
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
        lpfState_ = hpfState_ = bpfState1_ = bpfState2_ = 0.0f;

#if HAS_RTNEURAL
        model_.reset();
        // Note: Model weights need to be loaded via LoadModelFromJson()
#endif
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            inputGain_ = v;
            break;
        case 1:
            outputGain_ = v;
            break;
        case 2:
            bass_ = v;
            break;
        case 3:
            mid_ = v;
            break;
        case 4:
            treble_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, (uint8_t)(inputGain_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(outputGain_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(bass_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(mid_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(treble_ * 127.0f + 0.5f)};
        return 5;
    }

    // Get the name of the currently loaded model
    const std::string &GetModelName() const { return modelName_; }

    // Get the path to the currently loaded model
    const std::string &GetModelPath() const { return modelPath_; }

    // Check if a model is loaded and ready
    bool IsModelLoaded() const { return modelLoaded_; }

    // Simple utility functions
    static inline float fclamp(float x, float lo, float hi)
    {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static inline float dBToLinear(float dB)
    {
        return std::pow(10.0f, dB / 20.0f);
    }

    void ProcessStereo(float &l, float &r) override
    {
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

        // Simple 3-band EQ (Baxandall-style)
        // This is a simple post-model tone shaping
        float bassCoeff = 0.05f + 0.15f * bass_;
        float trebCoeff = 0.1f + 0.4f * treble_;

        // Low shelf
        lpfState_ += bassCoeff * (mono - lpfState_);
        float bassBoost = (bass_ - 0.5f) * 2.0f; // -1 to +1
        float lowShelf = mono + bassBoost * (lpfState_ - mono);

        // High shelf
        hpfState_ += trebCoeff * (lowShelf - hpfState_);
        float trebBoost = (treble_ - 0.5f) * 2.0f;
        float highShelf = lowShelf + trebBoost * (lowShelf - hpfState_);

        // Mid band (simple bandpass)
        float midFreq = 800.0f / sampleRate_;
        bpfState1_ += midFreq * (highShelf - bpfState1_);
        bpfState2_ += midFreq * (bpfState1_ - bpfState2_);
        float midBoost = (mid_ - 0.5f) * 2.0f;
        float output = highShelf + midBoost * (bpfState1_ - bpfState2_);

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
