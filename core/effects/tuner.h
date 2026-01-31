#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"

/**
 * TunerEffect - Mute-only tuner optimized for guitar (E2–E6)
 *
 * - Output is always muted.
 * - Uses decimated AMDF (absolute mean difference) to estimate pitch.
 * - Readonly output params: Note (enum 0-11) and Cents (-50 to +50).
 * - Marked isGlobal: takes exclusive audio routing when enabled.
 */
struct TunerEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Tuner::TypeId;

    // Guitar-focused detection range (E2–E6 with guard band)
    static constexpr float kMinHz = 70.0f;
    static constexpr float kMaxHz = 1400.0f;

    // Decimation and windowing (small CPU)
    static constexpr int kDecimation = 4;      // 48k -> 12k
    static constexpr int kWindowSize = 1024;   // ~85ms @ 12k
    static constexpr int kHopSize = 256;       // Update every ~21ms @ 12k
    static constexpr float kMinSignal = 0.02f; // Minimum peak level to trust detection

    // Ring buffer for decimated samples
    float buffer_[kWindowSize] = {0.0f};
    int writeIdx_ = 0;
    int decimCounter_ = 0;
    int filled_ = 0;
    int hopCounter_ = 0;

    float sampleRate_ = 48000.0f;
    float decimatedRate_ = 12000.0f;

    // Last detected pitch (Hz) and confidence (0..1)
    float lastPitchHz_ = 0.0f;
    float lastConfidence_ = 0.0f;

    // Derived tuner output: note index (0-11) and cents offset (-50..+50)
    // -1 = no detection (sentinel for UI to show "--")
    float lastNoteIndex_ = -1.0f;
    float lastCentsOffset_ = 0.0f;

    const EffectMeta &GetMetadata() const override { return Effects::Tuner::kMeta; }
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        decimatedRate_ = sr / static_cast<float>(kDecimation);
        writeIdx_ = 0;
        decimCounter_ = 0;
        filled_ = 0;
        hopCounter_ = 0;
        lastPitchHz_ = 0.0f;
        lastConfidence_ = 0.0f;
        lastNoteIndex_ = -1.0f;
        lastCentsOffset_ = 0.0f;
        for (int i = 0; i < kWindowSize; ++i)
            buffer_[i] = 0.0f;
    }

    void SetParam(uint8_t, float) override {}

    uint8_t GetParamsSnapshot(ParamDesc *, uint8_t) const override { return 0; }

    void ProcessStereo(float &l, float &r) override
    {
        // Mono analysis signal (pre-gain already applied by AudioProcessor)
        float m = 0.5f * (l + r);

        // Decimate to reduce CPU
        if (++decimCounter_ >= kDecimation)
        {
            decimCounter_ = 0;
            buffer_[writeIdx_] = m;
            writeIdx_ = (writeIdx_ + 1) % kWindowSize;
            if (filled_ < kWindowSize)
                filled_++;

            if (++hopCounter_ >= kHopSize)
            {
                hopCounter_ = 0;
                if (filled_ >= kWindowSize)
                    EstimatePitch();
            }
        }

        // Mute output (tuner only)
        l = 0.0f;
        r = 0.0f;
    }

    // Accessors for UI/telemetry (optional)
    float GetLastPitchHz() const { return lastPitchHz_; }
    float GetLastConfidence() const { return lastConfidence_; }

    uint8_t GetOutputParams(OutputParamDesc *out, uint8_t max) const override
    {
        if (max < 2)
            return 0;
        out[0] = {0, lastNoteIndex_};
        out[1] = {1, lastCentsOffset_};
        return 2;
    }

private:
    inline float GetSample(int i) const
    {
        // Oldest sample is at writeIdx_ (next write position)
        int idx = writeIdx_ + i;
        if (idx >= kWindowSize)
            idx -= kWindowSize;
        return buffer_[idx];
    }

    float ComputeAMDF(int lag) const
    {
        float sum = 0.0f;
        int limit = kWindowSize - lag;
        for (int i = 0; i < limit; ++i)
        {
            float diff = GetSample(i) - GetSample(i + lag);
            sum += FastMath::fabs(diff);
        }
        return sum;
    }

    void EstimatePitch()
    {
        // Quick level check
        float peak = 0.0f;
        float sumAbs = 0.0f;
        for (int i = 0; i < kWindowSize; ++i)
        {
            float v = FastMath::fabs(GetSample(i));
            sumAbs += v;
            peak = FastMath::fmax(peak, v);
        }
        if (peak < kMinSignal)
        {
            lastPitchHz_ = 0.0f;
            lastConfidence_ = 0.0f;
            lastNoteIndex_ = -1.0f;
            lastCentsOffset_ = 0.0f;
            return;
        }

        int minLag = static_cast<int>(decimatedRate_ / kMaxHz);
        int maxLag = static_cast<int>(decimatedRate_ / kMinHz);
        if (minLag < 2)
            minLag = 2;
        if (maxLag > kWindowSize - 2)
            maxLag = kWindowSize - 2;
        if (minLag >= maxLag)
            return;

        float bestVal = 1e30f;
        int bestLag = minLag;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float val = ComputeAMDF(lag);
            if (val < bestVal)
            {
                bestVal = val;
                bestLag = lag;
            }
        }

        // Parabolic interpolation for sub-sample refinement
        float refinedLag = static_cast<float>(bestLag);
        if (bestLag > minLag && bestLag < maxLag)
        {
            float d1 = ComputeAMDF(bestLag - 1);
            float d2 = bestVal;
            float d3 = ComputeAMDF(bestLag + 1);
            float denom = (d1 + d3 - 2.0f * d2);
            if (denom != 0.0f)
            {
                float delta = 0.5f * (d1 - d3) / denom;
                refinedLag = static_cast<float>(bestLag) + delta;
            }
        }

        if (refinedLag <= 0.0f)
            return;

        float freq = decimatedRate_ / refinedLag;

        // Confidence estimate: lower AMDF vs average amplitude
        float norm = sumAbs * static_cast<float>(kWindowSize - bestLag) + 1e-6f;
        float confidence = 1.0f - (bestVal / norm);
        confidence = FastMath::fclamp(confidence, 0.0f, 1.0f);

        // Light smoothing for stability
        lastPitchHz_ = (lastPitchHz_ <= 0.0f) ? freq : (0.8f * lastPitchHz_ + 0.2f * freq);
        lastConfidence_ = (0.7f * lastConfidence_) + (0.3f * confidence);

        // Convert Hz to nearest note + cents offset
        // MIDI note = 12 * log2(hz / 440) + 69, note index = midi % 12
        if (lastPitchHz_ > 0.0f)
        {
            float semitones = 12.0f * FastMath::fastLog2(lastPitchHz_ / 440.0f) + 69.0f;
            float nearestNote = static_cast<float>(static_cast<int>(semitones + 0.5f));
            lastCentsOffset_ = (semitones - nearestNote) * 100.0f;
            int noteIdx = static_cast<int>(nearestNote) % 12;
            if (noteIdx < 0)
                noteIdx += 12;
            lastNoteIndex_ = static_cast<float>(noteIdx);
        }
    }
};
