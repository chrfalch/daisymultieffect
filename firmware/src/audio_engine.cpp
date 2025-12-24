#include "audio_engine.h"

AudioEngine::AudioEngine(TempoSource &tempo)
    : tempo_(tempo), board_{}, fx_delay_(tempo_), fx_sweep_(tempo_), fx_distA_{}, fx_distB_{}, fx_stMix_{}, fx_reverb_{}
{
}

void AudioEngine::Init(float sample_rate)
{
    board_.sampleRate = sample_rate;
}

static inline float ReadTap(uint8_t idx, float input, const float *buf)
{
    return (idx == ROUTE_INPUT) ? input : buf[idx];
}

BaseEffect *AudioEngine::Instantiate(uint8_t typeId, int slotIndex)
{
    (void)slotIndex;
    switch (typeId)
    {
    case DelayEffect::TypeId:
        return &fx_delay_;
    case StereoSweepDelayEffect::TypeId:
        return &fx_sweep_;
    case DistortionEffect::TypeId:
        return (slotIndex == 1 ? &fx_distA_ : &fx_distB_);
    case StereoMixerEffect::TypeId:
        return &fx_stMix_;
    case SimpleReverbEffect::TypeId:
        return &fx_reverb_;
    default:
        return nullptr;
    }
}

void AudioEngine::ApplyPatch(const PatchWireDesc &pw)
{
    for (int i = 0; i < 12; i++)
    {
        auto &s = board_.slots[i];
        s.effect = nullptr;
        s.typeId = 0;
    }

    for (uint8_t i = 0; i < pw.numSlots && i < 12; i++)
    {
        const SlotWireDesc &sw = pw.slots[i];
        auto &rt = board_.slots[i];

        rt.typeId = sw.typeId;
        rt.enabled = sw.enabled != 0;
        rt.inputL = sw.inputL;
        rt.inputR = sw.inputR;
        rt.sumToMono = sw.sumToMono != 0;
        rt.dry = sw.dry / 127.0f;
        rt.wet = sw.wet / 127.0f;
        rt.policy = (ChannelPolicy)sw.channelPolicy;

        rt.effect = Instantiate(sw.typeId, (int)sw.slotIndex);
        if (rt.effect)
        {
            rt.effect->Init(board_.sampleRate);
            for (uint8_t p = 0; p < sw.numParams && p < 8; p++)
                rt.effect->SetParam(sw.params[p].id, sw.params[p].value / 127.0f);
        }
    }
}

void AudioEngine::ProcessFrame(float inL, float inR, float &outL, float &outR)
{
    board_.ResetFrameBuffers();

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

        float srcL = ReadTap(s.inputL, inL, board_.outL);
        float srcR = ReadTap(s.inputR, inR, board_.outR);
        if (s.sumToMono)
        {
            float m = 0.5f * (srcL + srcR);
            srcL = m;
            srcR = m;
        }

        float wetL = srcL, wetR = srcR;
        if (s.enabled)
            s.effect->ProcessStereo(wetL, wetR);

        float yL = srcL * s.dry + wetL * s.wet;
        float yR = srcR * s.dry + wetR * s.wet;

        board_.outL[i] = yL;
        board_.outR[i] = yR;
        curL = yL;
        curR = yR;
    }

    outL = curL;
    outR = curR;
}

void AudioEngine::ProcessBlock(daisy::AudioHandle::InputBuffer in,
                               daisy::AudioHandle::OutputBuffer out,
                               size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float inL = in[0][i];
        float inR = in[1][i];
        float outL, outR;
        ProcessFrame(inL, inR, outL, outR);
        out[0][i] = outL;
        out[1][i] = outR;
    }
}
