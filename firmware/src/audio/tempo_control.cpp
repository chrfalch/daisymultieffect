#include "audio/tempo_control.h"

#include "midi/midi_control.h"

namespace
{
    constexpr uint32_t kMinTapIntervalUs = 100000;  // 0.1s => 600 BPM
    constexpr uint32_t kMaxTapIntervalUs = 2000000; // 2.0s => 30 BPM
    constexpr float kMinBpm = 40.0f;
    constexpr float kMaxBpm = 240.0f;
} // namespace

void TempoControl::Init(TempoSource &tempo, MidiControl *midi)
{
    tempo_ = &tempo;
    midi_ = midi;
}

void TempoControl::Reset()
{
    lastTapUs_ = 0;
    avgTapUs_ = 0;
}

void TempoControl::Tap(uint32_t nowUs)
{
    if (!tempo_)
        return;

    if (lastTapUs_ == 0)
    {
        lastTapUs_ = nowUs;
        avgTapUs_ = 0;
        return;
    }

    const uint32_t dt = nowUs - lastTapUs_;
    lastTapUs_ = nowUs;

    if (dt < kMinTapIntervalUs || dt > kMaxTapIntervalUs)
    {
        avgTapUs_ = 0;
        return;
    }

    avgTapUs_ = (avgTapUs_ == 0) ? dt : (avgTapUs_ * 3 + dt) / 4;

    float sec = (float)avgTapUs_ / 1e6f;
    float bpm = 60.0f / (sec > 1e-6f ? sec : 1e-6f);

    if (bpm < kMinBpm)
        bpm = kMinBpm;
    if (bpm > kMaxBpm)
        bpm = kMaxBpm;

    tempo_->bpm = bpm;
    tempo_->valid = true;

    if (midi_)
        midi_->SendTempoUpdate(bpm);
}
