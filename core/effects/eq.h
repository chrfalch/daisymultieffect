#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

/**
 * GraphicEQEffect - 7-band graphic equalizer optimized for guitar
 *
 * Frequencies chosen for guitar tone shaping:
 * - 100 Hz:  Bass/thump - adds low-end weight
 * - 200 Hz:  Warmth/body - fullness of tone
 * - 400 Hz:  Low-mids - can reduce boxiness
 * - 800 Hz:  Midrange - honk/punch
 * - 1.6 kHz: Upper-mids - cut/bite/attack
 * - 3.2 kHz: Presence - clarity/definition
 * - 6.4 kHz: Treble - sparkle/air
 *
 * Each band: -12dB to +12dB gain
 */
struct GraphicEQEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::GraphicEQ::TypeId;
    static constexpr int NUM_BANDS = 7;

    // Biquad filter state for one channel
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void Reset()
        {
            z1 = z2 = 0.0f;
        }

        float Process(float in)
        {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        // Peaking EQ filter design using fast trig
        void SetPeakingEQ(float sr, float freq, float gainDb, float Q)
        {
            float A = FastMath::fastDbToLin(gainDb * 0.5f); // sqrt of linear gain
            float w0 = FastMath::kTwoPi * freq / sr;
            float phase = w0 * (1.0f / FastMath::kTwoPi); // Convert to normalized phase
            float cosw0 = FastMath::fastCos(phase);
            float sinw0 = FastMath::fastSin(phase);
            float alpha = sinw0 / (2.0f * Q);

            float a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0;
            b1 = (-2.0f * cosw0) / a0;
            b2 = (1.0f - alpha * A) / a0;
            a1 = b1; // Same as b1 for peaking
            a2 = (1.0f - alpha / A) / a0;
        }
    };

    // Filter state: NUM_BANDS x 2 channels
    Biquad filtersL_[NUM_BANDS];
    Biquad filtersR_[NUM_BANDS];

    // Band gains in dB (-12 to +12), stored as 0..1 (0.5 = 0dB)
    float gains_[NUM_BANDS] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Dirty flag for deferred filter coefficient updates
    bool filtersDirty_ = true;

    float sampleRate_ = 48000.0f;

    // Band center frequencies (Hz)
    static float GetFrequency(int band)
    {
        static const float freqs[NUM_BANDS] = {
            100.0f,  // Bass
            200.0f,  // Warmth
            400.0f,  // Low-mid
            800.0f,  // Midrange
            1600.0f, // Upper-mid
            3200.0f, // Presence
            6400.0f  // Treble
        };
        return freqs[band];
    }

    // Q values (bandwidth)
    static float GetQ(int band)
    {
        static const float qs[NUM_BANDS] = {
            1.0f, // 100 Hz - wider
            1.2f, // 200 Hz
            1.4f, // 400 Hz
            1.4f, // 800 Hz
            1.4f, // 1.6 kHz
            1.2f, // 3.2 kHz
            1.0f  // 6.4 kHz - wider
        };
        return qs[band];
    }

    const EffectMeta &GetMetadata() const override { return Effects::GraphicEQ::kMeta; }
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        for (int i = 0; i < NUM_BANDS; i++)
        {
            filtersL_[i].Reset();
            filtersR_[i].Reset();
        }
        filtersDirty_ = true;
        UpdateAllFilters();
    }

    void SetParam(uint8_t id, float v) override
    {
        if (id < NUM_BANDS)
        {
            gains_[id] = v;
            filtersDirty_ = true; // Defer coefficient update to ProcessStereo
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < NUM_BANDS)
            return 0;
        for (int i = 0; i < NUM_BANDS; i++)
        {
            out[i] = {(uint8_t)i, (uint8_t)(gains_[i] * 127.0f + 0.5f)};
        }
        return NUM_BANDS;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Update filter coefficients if dirty (deferred from SetParam)
        if (filtersDirty_)
        {
            UpdateAllFilters();
            filtersDirty_ = false;
        }

        // Process through all bands in series
        float outL = l;
        float outR = r;

        for (int i = 0; i < NUM_BANDS; i++)
        {
            outL = filtersL_[i].Process(outL);
            outR = filtersR_[i].Process(outR);
        }

        l = outL;
        r = outR;
    }

private:
    void UpdateFilter(int band)
    {
        // Convert 0..1 to -12..+12 dB
        float gainDb = (gains_[band] - 0.5f) * 24.0f;

        filtersL_[band].SetPeakingEQ(sampleRate_, GetFrequency(band), gainDb, GetQ(band));
        filtersR_[band].SetPeakingEQ(sampleRate_, GetFrequency(band), gainDb, GetQ(band));
    }

    void UpdateAllFilters()
    {
        for (int i = 0; i < NUM_BANDS; i++)
        {
            UpdateFilter(i);
        }
    }
};
