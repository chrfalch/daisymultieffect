#pragma once

#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include "effects/filters/biquad.h"

/**
 * Leslie Rotating Speaker Cabinet Simulator
 * 
 * Simulates the classic Leslie speaker effect used with Hammond organs.
 * Features:
 * - Separate horn and bass rotors with independent speeds
 * - Doppler pitch modulation (speakers moving toward/away from listener)
 * - Amplitude modulation (volume changes based on direction)
 * - Stereo imaging (spatial movement)
 * - 4th-order Butterworth crossover (separates highs/lows)
 * - Smooth acceleration between speed modes
 * - Input drive and wet/dry mix
 * 
 * Speed presets match Leslie 122 specifications:
 * - Horn: Slow=42 RPM (0.7 Hz), Fast=400 RPM (6.67 Hz)
 * - Bass: Slow=30 RPM (0.5 Hz), Fast=324 RPM (5.4 Hz)
 * 
 * Physical constants:
 * - Horn radius: 0.19 m
 * - Bass radius: 0.24 m
 * - Speed of sound: 343 m/s
 */
struct LeslieEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Leslie::TypeId;
    
    // Delay buffer size for Doppler simulation (512 samples ~10.7ms at 48kHz)
    static constexpr int DELAY_BUF_SIZE = 512;
    
    // Physical constants
    static constexpr float SPEED_OF_SOUND = 343.0f; // m/s
    static constexpr float HORN_RADIUS = 0.19f;     // meters
    static constexpr float BASS_RADIUS = 0.24f;     // meters
    
    // Speed modes
    enum SpeedMode
    {
        SPEED_STOP = 0,
        SPEED_SLOW = 1,
        SPEED_FAST = 2
    };
    
    // Delay buffers for Doppler effect (4 total: horn L/R, bass L/R)
    float hornDelayL_[DELAY_BUF_SIZE] = {0};
    float hornDelayR_[DELAY_BUF_SIZE] = {0};
    float bassDelayL_[DELAY_BUF_SIZE] = {0};
    float bassDelayR_[DELAY_BUF_SIZE] = {0};
    int delayWriteIdx_ = 0;
    
    // 4th-order Butterworth crossover (2 biquads per channel per band = 8 total)
    Biquad hornHP1L_, hornHP2L_, hornHP1R_, hornHP2R_; // 4th-order highpass for horn
    Biquad bassLP1L_, bassLP2L_, bassLP1R_, bassLP2R_; // 4th-order lowpass for bass
    
    // Rotor state
    float hornAngle_ = 0.0f;      // Horn rotor angle (radians)
    float bassAngle_ = 0.0f;      // Bass rotor angle (radians)
    float hornSpeed_ = 0.0f;      // Current horn speed (Hz)
    float bassSpeed_ = 0.0f;      // Current bass speed (Hz)
    float hornTargetSpeed_ = 0.0f; // Target horn speed
    float bassTargetSpeed_ = 0.0f; // Target bass speed
    
    // Speed presets (Hz)
    float hornSpeedSlow_ = 0.7f;   // 42 RPM
    float hornSpeedFast_ = 6.67f;  // 400 RPM
    float bassSpeedSlow_ = 0.5f;   // 30 RPM
    float bassSpeedFast_ = 5.4f;   // 324 RPM
    
    // Acceleration smoothing coefficient
    float accelCoeff_ = 0.01f;
    
    // Parameters (normalized 0-1, except speed mode which is 0-2)
    SpeedMode speedMode_ = SPEED_STOP;
    float acceleration_ = 0.4f;   // id1: 0.1-5.0s mapped to 0-1
    float separation_ = 0.5f;     // id2: stereo width 0-100%
    float hornLevel_ = 0.7f;      // id3: horn mix 0-100%
    float bassLevel_ = 0.7f;      // id4: bass mix 0-100%
    float crossover_ = 0.5f;      // id5: 400-1200 Hz mapped to 0-1
    float drive_ = 0.0f;          // id6: input saturation 0-100%
    float mix_ = 1.0f;            // id7: wet/dry 0-100%
    
    float sampleRate_ = 48000.0f;
    
    const EffectMeta &GetMetadata() const override { return Effects::Leslie::kMeta; }
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }
    
    void Init(float sr) override
    {
        sampleRate_ = sr;
        
        // Calculate speed presets from RPM
        hornSpeedSlow_ = 42.0f / 60.0f;   // 42 RPM -> Hz
        hornSpeedFast_ = 400.0f / 60.0f;  // 400 RPM -> Hz
        bassSpeedSlow_ = 30.0f / 60.0f;   // 30 RPM -> Hz
        bassSpeedFast_ = 324.0f / 60.0f;  // 324 RPM -> Hz
        
        // Clear delay buffers
        for (int i = 0; i < DELAY_BUF_SIZE; i++)
        {
            hornDelayL_[i] = 0.0f;
            hornDelayR_[i] = 0.0f;
            bassDelayL_[i] = 0.0f;
            bassDelayR_[i] = 0.0f;
        }
        delayWriteIdx_ = 0;
        
        // Reset rotor state
        hornAngle_ = 0.0f;
        bassAngle_ = 0.0f;
        hornSpeed_ = 0.0f;
        bassSpeed_ = 0.0f;
        
        // Reset crossover filters
        hornHP1L_.Reset();
        hornHP2L_.Reset();
        hornHP1R_.Reset();
        hornHP2R_.Reset();
        bassLP1L_.Reset();
        bassLP2L_.Reset();
        bassLP1R_.Reset();
        bassLP2R_.Reset();
        
        // Initialize filters and parameters
        UpdateCrossover();
        UpdateAcceleration();
        UpdateTargetSpeeds();
    }
    
    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Speed mode (0=stop, 1=slow, 2=fast)
            {
                int mode = (int)(v * 2.0f + 0.5f); // Map 0-1 to 0-2
                if (mode < 0) mode = 0;
                if (mode > 2) mode = 2;
                speedMode_ = (SpeedMode)mode;
                UpdateTargetSpeeds();
            }
            break;
        case 1: // Acceleration (0.1-5.0s)
            acceleration_ = v;
            UpdateAcceleration();
            break;
        case 2: // Separation (stereo width)
            separation_ = v;
            break;
        case 3: // Horn level
            hornLevel_ = v;
            break;
        case 4: // Bass level
            bassLevel_ = v;
            break;
        case 5: // Crossover frequency
            crossover_ = v;
            UpdateCrossover();
            break;
        case 6: // Drive
            drive_ = v;
            break;
        case 7: // Mix
            mix_ = v;
            break;
        }
    }
    
    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 8)
            return 0;
        out[0] = {0, (uint8_t)(((int)speedMode_ / 2.0f) * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(acceleration_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(separation_ * 127.0f + 0.5f)};
        out[3] = {3, (uint8_t)(hornLevel_ * 127.0f + 0.5f)};
        out[4] = {4, (uint8_t)(bassLevel_ * 127.0f + 0.5f)};
        out[5] = {5, (uint8_t)(crossover_ * 127.0f + 0.5f)};
        out[6] = {6, (uint8_t)(drive_ * 127.0f + 0.5f)};
        out[7] = {7, (uint8_t)(mix_ * 127.0f + 0.5f)};
        return 8;
    }
    
    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        float dryL = l, dryR = r;
        
        // 1. Optional input drive (soft saturation)
        if (drive_ > 0.01f)
        {
            float driveAmount = drive_ * 3.0f; // Scale drive
            l = SoftClip(l * (1.0f + driveAmount)) / (1.0f + driveAmount * 0.5f);
            r = SoftClip(r * (1.0f + driveAmount)) / (1.0f + driveAmount * 0.5f);
        }
        
        // 2. Crossover filtering
        // Horn path: 4th-order highpass
        float hornL = hornHP1L_.Process(l);
        hornL = hornHP2L_.Process(hornL);
        float hornR = hornHP1R_.Process(r);
        hornR = hornHP2R_.Process(hornR);
        
        // Bass path: 4th-order lowpass
        float bassL = bassLP1L_.Process(l);
        bassL = bassLP2L_.Process(bassL);
        float bassR = bassLP1R_.Process(r);
        bassR = bassLP2R_.Process(bassR);
        
        // 3. Speed smoothing (exponential approach to target)
        hornSpeed_ += (hornTargetSpeed_ - hornSpeed_) * accelCoeff_;
        bassSpeed_ += (bassTargetSpeed_ - bassSpeed_) * accelCoeff_;
        
        // 4. Update rotor angles
        float hornInc = FastMath::kTwoPi * hornSpeed_ / sampleRate_;
        float bassInc = FastMath::kTwoPi * bassSpeed_ / sampleRate_;
        hornAngle_ += hornInc;
        bassAngle_ += bassInc;
        
        // Wrap angles to [0, 2π)
        if (hornAngle_ >= FastMath::kTwoPi)
            hornAngle_ -= FastMath::kTwoPi;
        if (bassAngle_ >= FastMath::kTwoPi)
            bassAngle_ -= FastMath::kTwoPi;
        
        // 5. Process horn rotor
        float hornOutL, hornOutR;
        ProcessRotor(hornL, hornR, hornAngle_, HORN_RADIUS, 
                    hornDelayL_, hornDelayR_, hornOutL, hornOutR);
        
        // 6. Process bass rotor
        float bassOutL, bassOutR;
        ProcessRotor(bassL, bassR, bassAngle_, BASS_RADIUS,
                    bassDelayL_, bassDelayR_, bassOutL, bassOutR);
        
        // 7. Mix rotors with level controls
        float wetL = hornOutL * hornLevel_ + bassOutL * bassLevel_;
        float wetR = hornOutR * hornLevel_ + bassOutR * bassLevel_;
        
        // 8. Wet/dry mix
        l = dryL * (1.0f - mix_) + wetL * mix_;
        r = dryR * (1.0f - mix_) + wetR * mix_;
        
        // Advance delay write pointer
        delayWriteIdx_ = (delayWriteIdx_ + 1) % DELAY_BUF_SIZE;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
    
private:
    // Update crossover filter coefficients
    void UpdateCrossover()
    {
        // Map 0-1 to 400-1200 Hz
        float freq = 400.0f + crossover_ * 800.0f;
        
        // Configure 4th-order Butterworth (two cascaded 2nd-order)
        // Horn: highpass
        hornHP1L_.SetHighpass(freq, sampleRate_);
        hornHP2L_.SetHighpass(freq, sampleRate_);
        hornHP1R_.SetHighpass(freq, sampleRate_);
        hornHP2R_.SetHighpass(freq, sampleRate_);
        
        // Bass: lowpass
        bassLP1L_.SetLowpass(freq, sampleRate_);
        bassLP2L_.SetLowpass(freq, sampleRate_);
        bassLP1R_.SetLowpass(freq, sampleRate_);
        bassLP2R_.SetLowpass(freq, sampleRate_);
    }
    
    // Update acceleration smoothing coefficient
    void UpdateAcceleration()
    {
        // Map 0-1 to 0.1-5.0 seconds
        float accelTime = 0.1f + acceleration_ * 4.9f;
        
        // Convert to smoothing coefficient (exponential approach)
        // tau = accelTime, alpha = 1 - exp(-1/(tau * sampleRate))
        // Simplified: alpha ≈ 1 / (tau * sampleRate) for small values
        accelCoeff_ = 1.0f / (accelTime * sampleRate_);
        
        // Clamp to reasonable range
        if (accelCoeff_ < 0.0001f)
            accelCoeff_ = 0.0001f;
        if (accelCoeff_ > 0.1f)
            accelCoeff_ = 0.1f;
    }
    
    // Update target speeds based on speed mode
    void UpdateTargetSpeeds()
    {
        switch (speedMode_)
        {
        case SPEED_STOP:
            hornTargetSpeed_ = 0.0f;
            bassTargetSpeed_ = 0.0f;
            break;
        case SPEED_SLOW:
            hornTargetSpeed_ = hornSpeedSlow_;
            bassTargetSpeed_ = bassSpeedSlow_;
            break;
        case SPEED_FAST:
            hornTargetSpeed_ = hornSpeedFast_;
            bassTargetSpeed_ = bassSpeedFast_;
            break;
        }
    }
    
    // Process a single rotor with Doppler and amplitude modulation
    void ProcessRotor(float inL, float inR, float angle, float radius,
                     float *delayBufL, float *delayBufR,
                     float &outL, float &outR)
#if !defined(DAISY_SEED_BUILD)
    {
        // Virtual microphones at ±90° (left/right)
        const float micAngleL = FastMath::kHalfPi;      // 90° left
        const float micAngleR = -FastMath::kHalfPi;     // -90° right

        // Calculate Doppler delay in samples
        // delay = (radius / speedOfSound) * cos(angle_relative_to_mic) * sampleRate
        float dopplerScale = (radius / SPEED_OF_SOUND) * sampleRate_;

        // For left channel: angle relative to left microphone
        float angleRelL = angle - micAngleL;
        float dopplerDelayL = dopplerScale * FastMath::fastCos(angleRelL);

        // For right channel: angle relative to right microphone
        float angleRelR = angle - micAngleR;
        float dopplerDelayR = dopplerScale * FastMath::fastCos(angleRelR);

        // Base delay to keep all reads positive
        float baseDelay = DELAY_BUF_SIZE / 2.0f;
        float readDelayL = baseDelay + dopplerDelayL;
        float readDelayR = baseDelay + dopplerDelayR;

        // Read from delay buffers with interpolation
        float dopplerL = ReadDelayInterpolated(delayBufL, readDelayL);
        float dopplerR = ReadDelayInterpolated(delayBufR, readDelayR);

        // Amplitude modulation (volume changes based on direction)
        // 0.6 + 0.4*cos(angle_rel) gives range [0.2, 1.0]
        float ampModL = 0.6f + 0.4f * FastMath::fastCos(angleRelL);
        float ampModR = 0.6f + 0.4f * FastMath::fastCos(angleRelR);

        // Stereo panning (position in stereo field)
        // sin(angle) gives L/R position, scaled by separation
        float pan = FastMath::fastSin(angle) * separation_;
        float panL = 0.5f - pan * 0.5f; // 0=full left, 1=full right
        float panR = 0.5f + pan * 0.5f;

        // Combine modulations
        outL = dopplerL * ampModL * panL;
        outR = dopplerR * ampModR * panR;

        // Write input to delay buffers
        delayBufL[delayWriteIdx_] = inL;
        delayBufR[delayWriteIdx_] = inR;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
    
    // Read from delay buffer with linear interpolation
    float ReadDelayInterpolated(float *buf, float delaySamples)
    {
        // Clamp to valid range
        if (delaySamples < 1.0f)
            delaySamples = 1.0f;
        if (delaySamples > DELAY_BUF_SIZE - 2)
            delaySamples = DELAY_BUF_SIZE - 2;
        
        // Calculate read position
        float readPos = (float)delayWriteIdx_ - delaySamples;
        if (readPos < 0.0f)
            readPos += DELAY_BUF_SIZE;
        
        // Linear interpolation
        int idx0 = (int)readPos;
        int idx1 = (idx0 + 1) % DELAY_BUF_SIZE;
        float frac = readPos - (float)idx0;
        
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }
    
    // Soft clipper (cubic)
    float SoftClip(float x)
    {
        if (x > 1.0f)
            return 1.0f;
        if (x < -1.0f)
            return -1.0f;
        
        // Cubic soft clip: x - (x^3)/3
        return x - (x * x * x) / 3.0f;
    }
};
