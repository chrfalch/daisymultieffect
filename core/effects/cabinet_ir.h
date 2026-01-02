#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>
#include <cstring>
#include <string>

/**
 * Cabinet Impulse Response (IR) Effect
 *
 * Applies convolution with a loaded impulse response to simulate
 * guitar/bass speaker cabinets. Uses direct convolution which is
 * efficient for short IRs (up to 1024 samples at 48kHz = ~21ms).
 *
 * Typical guitar cab IRs are 20-50ms, so we support up to 2048 samples
 * for flexibility (43ms at 48kHz).
 *
 * For VST: IR can be loaded from WAV files at runtime
 * For Firmware: IR can be baked in at compile time
 */

struct CabinetIREffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::CabinetIR::TypeId;

    // Maximum IR length in samples (2048 @ 48kHz = 42.7ms)
    static constexpr int kMaxIRLength = 2048;

    // Parameters
    float mix_ = 1.0f;        // Wet/dry mix (0-1)
    float outputGain_ = 0.5f; // Output level (0-1 maps to -20dB to +20dB)
    float lowCut_ = 0.0f;     // High-pass filter (0-1 maps to 20Hz-500Hz)
    float highCut_ = 1.0f;    // Low-pass filter (0-1 maps to 2kHz-20kHz)

    // Internal state
    float sampleRate_ = 48000.0f;
    bool irLoaded_ = false;
    int irLength_ = 0;

    // IR data and name
    float irBuffer_[kMaxIRLength] = {0};
    std::string irName_ = "No IR";
    std::string irPath_;

    // Convolution input buffer (circular buffer for efficiency)
    float inputBuffer_[kMaxIRLength] = {0};
    int inputIndex_ = 0;

    // Simple filter states for low/high cut
    float hpfState_ = 0.0f;
    float lpfState_ = 0.0f;

    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }
    const EffectMeta &GetMetadata() const override { return Effects::CabinetIR::kMeta; }

    void Init(float sampleRate) override
    {
        sampleRate_ = sampleRate;

        // Clear convolution buffer
        std::memset(inputBuffer_, 0, sizeof(inputBuffer_));
        inputIndex_ = 0;

        // Reset filter states
        hpfState_ = 0.0f;
        lpfState_ = 0.0f;
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            mix_ = v;
            break;
        case 1:
            outputGain_ = v;
            break;
        case 2:
            lowCut_ = v;
            break;
        case 3:
            highCut_ = v;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 4)
            return 0;
        out[0] = {0, (uint8_t)(mix_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(outputGain_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(lowCut_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(highCut_ * 127.0f + 0.5f)};
        return 4;
    }

    // Get the name of the currently loaded IR
    const std::string &GetIRName() const { return irName_; }

    // Get the path to the currently loaded IR
    const std::string &GetIRPath() const { return irPath_; }

    // Check if an IR is loaded
    bool IsIRLoaded() const { return irLoaded_; }

    // Get IR length in samples
    int GetIRLength() const { return irLength_; }

    /**
     * Load IR data directly from a float array
     *
     * @param data Pointer to IR sample data (mono, normalized)
     * @param length Number of samples
     * @param name Display name for the IR
     * @param path Optional file path for reference
     * @return true if loaded successfully
     */
    bool LoadIR(const float *data, int length, const std::string &name = "Custom IR", const std::string &path = "")
    {
        if (!data || length <= 0)
        {
            irLoaded_ = false;
            irName_ = "Load Failed";
            return false;
        }

        // Clamp to max length
        if (length > kMaxIRLength)
            length = kMaxIRLength;

        // Copy IR data
        std::memcpy(irBuffer_, data, length * sizeof(float));
        irLength_ = length;

        // Normalize IR if needed (find peak and scale)
        float peak = 0.0f;
        for (int i = 0; i < irLength_; ++i)
        {
            float absVal = std::fabs(irBuffer_[i]);
            if (absVal > peak)
                peak = absVal;
        }

        if (peak > 0.0f && peak != 1.0f)
        {
            float scale = 1.0f / peak;
            for (int i = 0; i < irLength_; ++i)
                irBuffer_[i] *= scale;
        }

        // Clear input buffer for clean start
        std::memset(inputBuffer_, 0, sizeof(inputBuffer_));
        inputIndex_ = 0;

        irName_ = name;
        irPath_ = path;
        irLoaded_ = true;

        return true;
    }

    /**
     * Clear the loaded IR
     */
    void ClearIR()
    {
        std::memset(irBuffer_, 0, sizeof(irBuffer_));
        std::memset(inputBuffer_, 0, sizeof(inputBuffer_));
        irLength_ = 0;
        irLoaded_ = false;
        irName_ = "No IR";
        irPath_.clear();
        inputIndex_ = 0;
    }

    // Helper functions
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
        // Convert to mono for convolution (cab IRs are typically mono)
        float mono = 0.5f * (l + r);
        float dry = mono;

        float wet = 0.0f;

        if (irLoaded_ && irLength_ > 0)
        {
            // Store input sample in circular buffer
            inputBuffer_[inputIndex_] = mono;

            // Direct convolution
            // For efficiency, we only convolve up to irLength_ samples
            float sum = 0.0f;
            int bufIdx = inputIndex_;

            for (int i = 0; i < irLength_; ++i)
            {
                sum += inputBuffer_[bufIdx] * irBuffer_[i];

                // Wrap circular buffer index backwards
                if (--bufIdx < 0)
                    bufIdx = kMaxIRLength - 1;
            }

            wet = sum;

            // Advance input buffer index
            if (++inputIndex_ >= kMaxIRLength)
                inputIndex_ = 0;
        }
        else
        {
            // No IR loaded - pass through
            wet = mono;
        }

        // Apply high-pass filter (low cut)
        // lowCut_ 0-1 maps to 20Hz-800Hz (0 = no cut, 1 = cuts up to 800Hz)
        if (lowCut_ > 0.01f)
        {
            float hpFreq = 20.0f + lowCut_ * 780.0f;
            float hpCoeff = 1.0f - std::exp(-2.0f * 3.14159f * hpFreq / sampleRate_);
            hpfState_ += hpCoeff * (wet - hpfState_);
            wet = wet - hpfState_;
        }

        // Apply low-pass filter (high cut)
        // highCut_ 0-1 maps to 20kHz down to 1kHz (0 = no cut/bright, 1 = dark/1kHz)
        if (highCut_ > 0.01f)
        {
            // Invert: higher highCut_ = lower frequency = darker sound
            float lpFreq = 20000.0f - highCut_ * 19000.0f; // 20kHz -> 1kHz
            float lpCoeff = 1.0f - std::exp(-2.0f * 3.14159f * lpFreq / sampleRate_);
            lpfState_ += lpCoeff * (wet - lpfState_);
            wet = lpfState_;
        }

        // Apply output gain (-20dB to +20dB)
        float outGain = dBToLinear((outputGain_ - 0.5f) * 40.0f);
        wet *= outGain;

        // Mix wet/dry
        float output = dry * (1.0f - mix_) + wet * mix_;

        // Soft clip to prevent harsh clipping
        output = fclamp(output, -1.5f, 1.5f);

        // Output to stereo
        l = r = output;
    }
};
