#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include <cmath>
#include <cstring>
#include <string>

// Include shared embedded IR registry for both firmware and VST
#include "embedded/ir_registry.h"

/**
 * Convolution inner loop — the hottest path in cabinet IR processing.
 * On firmware, this is defined in cabinet_ir_itcmram.cpp with ITCMRAM placement.
 * On VST/desktop, the inline version below is used.
 */
#if defined(DAISY_SEED_BUILD)
float CabinetIR_Convolve(const float *inputBuf, int inputIdx,
                          const float *irBuf, int irLen, int maxLen);
#else
inline float CabinetIR_Convolve(const float *inputBuf, int inputIdx,
                                 const float *irBuf, int irLen, int maxLen)
{
    float sum = 0.0f;
    int bufIdx = inputIdx;
    for (int i = 0; i < irLen; ++i)
    {
        sum += inputBuf[bufIdx] * irBuf[i];
        if (--bufIdx < 0)
            bufIdx = maxLen - 1;
    }
    return sum;
}
#endif

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
 * Parameters:
 *   0: Cabinet - Enum selection of embedded cabinet IRs
 *   1: Mix - Wet/dry mix
 *   2: Output - Output level
 *   3: Low Cut - High-pass filter
 *   4: High Cut - Low-pass filter
 *
 * NOTE: V1 is mono only. Stereo IR support deferred to V2.
 *
 * For VST: IR can be loaded from WAV files at runtime OR from embedded data
 * For Firmware: IR loaded from embedded data via ir_registry.h
 */

struct CabinetIREffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::CabinetIR::TypeId;

    // Maximum IR length in samples
    // VST can handle longer IRs, firmware is limited for real-time performance
#if defined(DAISY_SEED_BUILD)
    // Firmware: 512 samples @ 48kHz = 10.7ms (captures essential cab character)
    // Direct convolution at 512 samples = ~24M MACs/sec (efficient on Cortex-M7)
    static constexpr int kMaxIRLength = 512;
#else
    // VST: Full 2048 samples @ 48kHz = 42.7ms
    static constexpr int kMaxIRLength = 2048;
#endif

    // Parameters
    uint8_t irIndex_ = 0;     // IR selection (enum)
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

    // Cached filter coefficients (updated when params change)
    float hpfCoeff_ = 0.0f;
    float lpfCoeff_ = 0.0f;
    float outGainLinear_ = 1.0f;
    bool coeffsNeedUpdate_ = true;

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

        // Mark coefficients for update
        coeffsNeedUpdate_ = true;

        // Load default embedded IR
        LoadEmbeddedIR(0);
    }

    // Update cached filter coefficients (called only when params change)
    void updateCoeffs()
    {
        // High-pass filter coefficient
        if (lowCut_ > 0.01f)
        {
            float hpFreq = 20.0f + lowCut_ * 780.0f;
            hpfCoeff_ = 1.0f - FastMath::fastExp(-2.0f * 3.14159f * hpFreq / sampleRate_);
        }
        else
        {
            hpfCoeff_ = 0.0f;
        }

        // Low-pass filter coefficient
        if (highCut_ > 0.01f)
        {
            float lpFreq = 20000.0f - highCut_ * 19000.0f;
            lpfCoeff_ = 1.0f - FastMath::fastExp(-2.0f * 3.14159f * lpFreq / sampleRate_);
        }
        else
        {
            lpfCoeff_ = 0.0f;
        }

        // Output gain
        outGainLinear_ = FastMath::fastDbToLin((outputGain_ - 0.5f) * 40.0f);

        coeffsNeedUpdate_ = false;
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0:
            // IR selection (enum parameter)
            {
                uint8_t newIndex = static_cast<uint8_t>(v * 127.0f + 0.5f);
                // Clamp to valid range
                if (newIndex >= EmbeddedIRs::kNumIRs)
                    newIndex = EmbeddedIRs::kNumIRs - 1;
                if (newIndex != irIndex_)
                {
                    irIndex_ = newIndex;
                    LoadEmbeddedIR(newIndex);
                }
            }
            break;
        case 1:
            mix_ = v;
            break;
        case 2:
            outputGain_ = v;
            coeffsNeedUpdate_ = true;
            break;
        case 3:
            lowCut_ = v;
            coeffsNeedUpdate_ = true;
            break;
        case 4:
            highCut_ = v;
            coeffsNeedUpdate_ = true;
            break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 5)
            return 0;
        out[0] = {0, irIndex_}; // Enum params use direct value
        out[1] = {1, (uint8_t)(mix_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(outputGain_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(lowCut_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(highCut_ * 127.0f + 0.5f)};
        return 5;
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

    /**
     * Load embedded IR by index from IR registry
     *
     * @param index Index into EmbeddedIRs::kIRRegistry
     * @return true if IR was loaded successfully
     */
    bool LoadEmbeddedIR(int index)
    {
        const auto *irInfo = EmbeddedIRs::GetIR(static_cast<size_t>(index));
        if (!irInfo || !irInfo->samples || irInfo->length <= 0)
        {
            irLoaded_ = false;
            irName_ = "Invalid IR";
            return false;
        }

        // Copy IR data (already normalized by converter)
        int length = irInfo->length;
        if (length > kMaxIRLength)
            length = kMaxIRLength;

        std::memcpy(irBuffer_, irInfo->samples, length * sizeof(float));
        irLength_ = length;

        // Clear input buffer for clean start
        std::memset(inputBuffer_, 0, sizeof(inputBuffer_));
        inputIndex_ = 0;

        irName_ = irInfo->name;
        irIndex_ = static_cast<uint8_t>(index);
        irLoaded_ = true;

        return true;
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
        // Update coefficients if needed (only when params change)
        if (coeffsNeedUpdate_)
            updateCoeffs();

        // Convert to mono for convolution (cab IRs are typically mono)
        float mono = 0.5f * (l + r);
        float dry = mono;

        float wet = 0.0f;

        if (irLoaded_ && irLength_ > 0)
        {
            // Store input sample in circular buffer
            inputBuffer_[inputIndex_] = mono;

            // Direct convolution (ITCMRAM-placed on firmware)
            wet = CabinetIR_Convolve(inputBuffer_, inputIndex_,
                                     irBuffer_, irLength_, kMaxIRLength);

            // Advance input buffer index
            if (++inputIndex_ >= kMaxIRLength)
                inputIndex_ = 0;
        }
        else
        {
            // No IR loaded - pass through
            wet = mono;
        }

        // Apply high-pass filter (low cut) using cached coefficient
        if (hpfCoeff_ > 0.0f)
        {
            hpfState_ += hpfCoeff_ * (wet - hpfState_);
            wet = wet - hpfState_;
        }

        // Apply low-pass filter (high cut) using cached coefficient
        if (lpfCoeff_ > 0.0f)
        {
            lpfState_ += lpfCoeff_ * (wet - lpfState_);
            wet = lpfState_;
        }

        // Apply output gain using cached linear value
        wet *= outGainLinear_;

        // Mix wet/dry
        float output = dry * (1.0f - mix_) + wet * mix_;

        // Soft clip to prevent harsh clipping
        output = fclamp(output, -1.5f, 1.5f);

        // Output to stereo
        l = r = output;
    }
};
