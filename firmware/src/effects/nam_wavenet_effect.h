/**
 * @file nam_wavenet_effect.h
 * @brief NAM WaveNet Pico Neural Amp Modeler for Daisy Seed firmware
 *
 * This is a dedicated NAM effect using WaveNet architecture (Mercury-style)
 * optimized for Cortex-M7. Uses Eigen backend for efficient scalar math.
 *
 * Architecture: WaveNet "Pico" (2 channels, kernel_size=3, 20 dilated layers)
 * - Much more efficient than LSTM on Cortex-M7
 * - Uses static memory allocation (no heap allocations during audio processing)
 * - Models bundled from GuitarML/Mercury project
 */
#pragma once

#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include <cmath>
#include <cstdint>
#include <string>

// This effect is only available on firmware with RTNeural
#if defined(DAISY_SEED_BUILD) && defined(HAS_RTNEURAL) && HAS_RTNEURAL

// Include WaveNet implementation and model weights
#include "nam_wavenet.h"
#include "nam_pico_weights.h"

/**
 * NamWavenetEffect - NAM WaveNet Pico Neural Amp Modeler
 *
 * Uses Mercury's WaveNet "Pico" architecture for efficient amp modeling
 * on Cortex-M7. Models are pre-trained and bundled in firmware.
 *
 * Parameters:
 *   0: Model - Enum selection (0-8 for 9 models)
 *   1: Input - Input gain/drive
 *   2: Output - Output level
 */
struct NamWavenetEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::NeuralAmp::TypeId;

    // Parameters
    uint8_t modelIndex_ = 255; // id0: Model selection (255 = not loaded yet)
    float inputGain_ = 0.5f;   // id1: Input gain/drive (0-1)
    float outputGain_ = 0.5f;  // id2: Output level (0-1)

    // Internal state
    float sampleRate_ = 48000.0f;
    bool modelLoaded_ = false;
    bool prewarmed_ = false;

    // Model name for display
    std::string modelName_ = "No Model";

    // WaveNet Pico model instance
    nam_wavenet::PicoModel model_;

    // Level adjustment (from Mercury: 0.4 * nnLevelAdjust)
    static constexpr float kLevelAdjust = 0.4f * 0.7f; // Conservative level

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::NeuralAmp::kMeta; }

    void Init(float sampleRate) override
    {
        sampleRate_ = sampleRate;

        // Reset model state
        model_.reset();

        // Load first model by default
        LoadModel(0);
    }

    /**
     * Prewarm the model to avoid initial transients
     * Should be called once at startup before audio processing begins
     * Takes approximately 0.3 seconds (16384 samples at 48kHz)
     */
    void Prewarm()
    {
        if (!modelLoaded_)
            return;

        // Run ~16k samples of zeros through the model
        constexpr int kPrewarmSamples = 16384;
        for (int i = 0; i < kPrewarmSamples; ++i)
        {
            model_.forward(0.0f);
        }
        prewarmed_ = true;
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
                if (newIndex >= nam_wavenet::kNamPicoModelCount)
                    newIndex = static_cast<uint8_t>(nam_wavenet::kNamPicoModelCount - 1);
                if (newIndex != modelIndex_)
                {
                    LoadModel(newIndex);
                }
            }
            break;
        case 1:
            inputGain_ = v;
            break;
        case 2:
            outputGain_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 4)
            return 0;
        out[0] = {0, static_cast<uint8_t>(modelIndex_ == 255 ? 0 : modelIndex_)};
        out[1] = {1, static_cast<uint8_t>(inputGain_ * 127.0f + 0.5f)};
        out[2] = {2, static_cast<uint8_t>(outputGain_ * 127.0f + 0.5f)};
        out[3] = {3, static_cast<uint8_t>(modelLoaded_ ? 127 : 0)}; // Status (read-only)
        return 4;
    }

    /**
     * Load a NAM Pico model by index
     * @param index Model index (0-8 for 9 models)
     * @return true if model loaded successfully
     */
    bool LoadModel(size_t index)
    {
        const auto *modelInfo = nam_wavenet::GetNamPicoModel(index);
        if (!modelInfo)
        {
            modelLoaded_ = false;
            modelName_ = "Invalid";
            return false;
        }

        // Load weights using std::span view (no copy)
        std::span<const float> weights(modelInfo->weights, modelInfo->weightCount);
        model_.load_weights(weights);
        model_.reset();

        modelLoaded_ = true;
        prewarmed_ = false;
        modelName_ = modelInfo->name;
        modelIndex_ = static_cast<uint8_t>(index);

        // Auto-prewarm after loading
        Prewarm();

        return true;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Convert to mono (amp models are mono)
        float mono = 0.5f * (l + r);

        // Apply input gain (-20dB to +20dB range)
        float inGain = FastMath::fastDbToLin((inputGain_ - 0.5f) * 40.0f);
        mono *= inGain;

        // Process through WaveNet model
        float output;
        if (modelLoaded_)
        {
            output = model_.forward(mono) * kLevelAdjust;
        }
        else
        {
            // Fallback: simple soft clipping
            output = FastMath::fast_tanh(mono * 2.0f) * 0.7f;
        }

        // Apply output gain (-20dB to +20dB range)
        float outGain = FastMath::fastDbToLin((outputGain_ - 0.5f) * 40.0f);
        output *= outGain;

        // Soft limit
        output = output < -1.5f ? -1.5f : (output > 1.5f ? 1.5f : output);

        // Output to stereo
        l = r = output;
    }

    // Accessors
    const std::string &GetModelName() const { return modelName_; }
    bool IsModelLoaded() const { return modelLoaded_; }
    bool IsPrewarmed() const { return prewarmed_; }
    size_t GetModelCount() const { return nam_wavenet::kNamPicoModelCount; }
};

#endif // DAISY_SEED_BUILD && HAS_RTNEURAL
