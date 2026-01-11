
#include "audio/audio_processor.h"

AudioProcessor::AudioProcessor(TempoSource &tempo)
    : tempo_(tempo),
      board_{},
      fx_delays_{{tempo_}, {tempo_}},
      fx_sweeps_{{tempo_}, {tempo_}},
      fx_dists_{},
      fx_mixers_{},
      fx_reverbs_{},
      fx_compressors_{},
      fx_choruses_{},
      fx_noisegates_{},
      fx_eqs_{},
      fx_flangers_{},
      fx_phasers_{},
      fx_neuralamps_{},
      fx_cabinetirs_{},
      delay_next_(0),
      sweep_next_(0),
      dist_next_(0),
      mixer_next_(0),
      reverb_next_(0),
      compressor_next_(0),
      chorus_next_(0),
      noisegate_next_(0),
      eq_next_(0),
      flanger_next_(0),
      phaser_next_(0),
      neuralamp_next_(0),
      cabinetir_next_(0)
{
}

void AudioProcessor::Init(float sampleRate)
{
    board_.sampleRate = sampleRate;
}

static inline float ReadTap(uint8_t idx, float input, const float *buf)
{
    return (idx == ROUTE_INPUT) ? input : buf[idx];
}

BaseEffect *AudioProcessor::Instantiate(uint8_t typeId, int slotIndex)
{
    (void)slotIndex;
    switch (typeId)
    {
    case DelayEffect::TypeId:
        if (delay_next_ < kMaxDelays)
            return &fx_delays_[delay_next_++];
        return nullptr;
    case StereoSweepDelayEffect::TypeId:
        if (sweep_next_ < kMaxSweeps)
            return &fx_sweeps_[sweep_next_++];
        return nullptr;
    case OverdriveEffect::TypeId:
        if (dist_next_ < kMaxDistortions)
            return &fx_dists_[dist_next_++];
        return nullptr;
    case StereoMixerEffect::TypeId:
        if (mixer_next_ < kMaxMixers)
            return &fx_mixers_[mixer_next_++];
        return nullptr;
    case SimpleReverbEffect::TypeId:
        if (reverb_next_ < kMaxReverbs)
            return &fx_reverbs_[reverb_next_++];
        return nullptr;
    case CompressorEffect::TypeId:
        if (compressor_next_ < kMaxCompressors)
            return &fx_compressors_[compressor_next_++];
        return nullptr;
    case ChorusEffect::TypeId:
        if (chorus_next_ < kMaxChoruses)
            return &fx_choruses_[chorus_next_++];
        return nullptr;
    case NoiseGateEffect::TypeId:
        if (noisegate_next_ < kMaxNoiseGates)
            return &fx_noisegates_[noisegate_next_++];
        return nullptr;
    case GraphicEQEffect::TypeId:
        if (eq_next_ < kMaxEQs)
            return &fx_eqs_[eq_next_++];
        return nullptr;
    case FlangerEffect::TypeId:
        if (flanger_next_ < kMaxFlangers)
            return &fx_flangers_[flanger_next_++];
        return nullptr;
    case PhaserEffect::TypeId:
        if (phaser_next_ < kMaxPhasers)
            return &fx_phasers_[phaser_next_++];
        return nullptr;
    case NeuralAmpEffect::TypeId:
        if (neuralamp_next_ < kMaxNeuralAmps)
            return &fx_neuralamps_[neuralamp_next_++];
        return nullptr;
    case CabinetIREffect::TypeId:
        if (cabinetir_next_ < kMaxCabinetIRs)
            return &fx_cabinetirs_[cabinetir_next_++];
        return nullptr;
    default:
        return nullptr;
    }
}

void AudioProcessor::ApplyPatch(const PatchWireDesc &pw)
{
    // Reset pool counters
    delay_next_ = 0;
    sweep_next_ = 0;
    dist_next_ = 0;
    mixer_next_ = 0;
    reverb_next_ = 0;
    compressor_next_ = 0;
    chorus_next_ = 0;
    noisegate_next_ = 0;
    eq_next_ = 0;
    flanger_next_ = 0;
    phaser_next_ = 0;
    neuralamp_next_ = 0;
    cabinetir_next_ = 0;

    // Clear all slots
    for (int i = 0; i < 12; i++)
    {
        auto &s = board_.slots[i];
        s.effect = nullptr;
        s.typeId = 0;
        s.enabled = true;
        s.enabledFade = 1.0f;
    }

    // Configure slots from patch
    for (uint8_t i = 0; i < pw.numSlots && i < 12; i++)
    {
        const SlotWireDesc &sw = pw.slots[i];
        auto &rt = board_.slots[i];

        rt.typeId = sw.typeId;
        rt.enabled = sw.enabled != 0;
        rt.enabledFade = rt.enabled ? 1.0f : 0.0f;
        rt.inputL = sw.inputL;
        rt.inputR = sw.inputR;
        rt.sumToMono = sw.sumToMono != 0;
        rt.dry = sw.dry / 127.0f;
        rt.wet = sw.wet / 127.0f;
        rt.policy = (ChannelPolicy)sw.channelPolicy;

        rt.effect = Instantiate(sw.typeId, (int)i);
        if (rt.effect)
        {
            rt.effect->Init(board_.sampleRate);
            for (uint8_t p = 0; p < sw.numParams && p < 8; p++)
                rt.effect->SetParam(sw.params[p].id, sw.params[p].value / 127.0f);
        }
    }
}

void AudioProcessor::ProcessFrame(float inL, float inR, float &outL, float &outR)
{
    board_.ResetFrameBuffers();

    // Apply input gain staging (boost instrument level to line level)
    inL *= inputGain_;
    inR *= inputGain_;

    // Track post-gain input peak level
    float inPeak = std::max(std::abs(inL), std::abs(inR));
    if (inPeak > inputPeakLevel_)
        inputPeakLevel_ = inPeak;

    // Fade time for bypass/enable transitions
    const float fadeSeconds = 0.005f; // 5ms
    const float fadeStep = (fadeSeconds > 0.0f && board_.sampleRate > 0.0f)
                               ? (1.0f / (fadeSeconds * board_.sampleRate))
                               : 1.0f;

    float curL = inL, curR = inR;

    for (int i = 0; i < 12; i++)
    {
        auto &s = board_.slots[i];
        if (!s.effect)
        {
            board_.outL[i] = curL;
            board_.outR[i] = curR;
            continue;
        }

        // Ramp enabledFade toward target
        const float target = s.enabled ? 1.0f : 0.0f;
        if (s.enabledFade < target)
        {
            s.enabledFade += fadeStep;
            if (s.enabledFade > 1.0f)
                s.enabledFade = 1.0f;
        }
        else if (s.enabledFade > target)
        {
            s.enabledFade -= fadeStep;
            if (s.enabledFade < 0.0f)
                s.enabledFade = 0.0f;
        }

        float srcL = ReadTap(s.inputL, inL, board_.outL);
        float srcR = ReadTap(s.inputR, inR, board_.outR);
        if (s.sumToMono)
        {
            float m = 0.5f * (srcL + srcR);
            srcL = m;
            srcR = m;
        }

        // Compute processed signal only when needed
        float procL = srcL, procR = srcR;
        if (s.enabledFade > 0.0f)
        {
            s.effect->ProcessStereo(procL, procR);

            // Apply channel policy - mono effects should duplicate output to both channels
            if (s.policy == ChannelPolicy::ForceMono ||
                (s.policy == ChannelPolicy::Auto && s.sumToMono))
            {
                float mono = 0.5f * (procL + procR);
                procL = mono;
                procR = mono;
            }
        }

        // Crossfade processed <-> bypassed
        const float g = s.enabledFade;
        float wetL = srcL * (1.0f - g) + procL * g;
        float wetR = srcR * (1.0f - g) + procR * g;

        // Wet/dry mix: dry=0,wet=1 means 100% effect, dry=1,wet=0 means bypass
        // If only wet is used (dry=0), treat wet as a crossfade mix control
        float yL, yR;
        if (s.dry > 0.0f)
        {
            // Traditional parallel blend: dry*src + wet*processed
            yL = srcL * s.dry + wetL * s.wet;
            yR = srcR * s.dry + wetR * s.wet;
        }
        else
        {
            // Single-knob mix: crossfade from dry to wet
            yL = srcL * (1.0f - s.wet) + wetL * s.wet;
            yR = srcR * (1.0f - s.wet) + wetR * s.wet;
        }

        board_.outL[i] = yL;
        board_.outR[i] = yR;
        curL = yL;
        curR = yR;
    }

    // Apply output gain staging
    outL = curL * outputGain_;
    outR = curR * outputGain_;

    // Track post-processing output peak level
    float outPeak = std::max(std::abs(outL), std::abs(outR));
    if (outPeak > outputPeakLevel_)
        outputPeakLevel_ = outPeak;
}
