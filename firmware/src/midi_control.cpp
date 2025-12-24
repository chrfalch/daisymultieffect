#include "midi_control.h"

#include <cstring>

#include "effect_registry.h"
#include "sysex_utils.h"

#include "effects_delay.h"
#include "effects_stereo_sweep_delay.h"
#include "effects_distortion.h"
#include "effects_stereo_mixer.h"
#include "effects_reverb.h"

uint8_t MidiControl::EncodeRoute(uint8_t r)
{
    // SysEx data bytes must be 7-bit.
    // Encode ROUTE_INPUT (255) as 127.
    return (r == ROUTE_INPUT) ? 127 : (uint8_t)(r & 0x7F);
}

void MidiControl::Init(daisy::DaisySeed &hw, Board &board, TempoSource &tempo)
{
    hw_ = &hw;
    board_ = &board;
    tempo_ = &tempo;

    daisy::MidiUsbTransport::Config cfg;
    cfg.periph = daisy::MidiUsbTransport::Config::INTERNAL;

    usb_midi_.Init(cfg);
    usb_midi_.StartRx(OnUsbMidiRx, this);
}

void MidiControl::SetCurrentPatch(const PatchWireDesc &patch)
{
    current_patch_ = patch;
}

void MidiControl::SendSysEx(const uint8_t *data, size_t len)
{
    // libDaisy expects a MIDI byte stream (status bytes included).
    // For SysEx, pass the full [0xF0..0xF7] payload.
    usb_midi_.Tx(const_cast<uint8_t *>(data), len);
}

void MidiControl::SendTempoUpdate(float bpm)
{
    uint8_t q[5];
    packQ16_16(floatToQ16_16(bpm), q);
    uint8_t msg[] = {0xF0, 0x7D, 0x41, q[0], q[1], q[2], q[3], q[4], 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendButtonStateChange(uint8_t btn, uint8_t slot, bool enabled)
{
    uint8_t msg[] = {0xF0, 0x7D, 0x40, btn, slot, (uint8_t)(enabled ? 1 : 0), 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::QueueSysexIfFree(const uint8_t *data, size_t len)
{
    if (sysex_ready_)
        return;
    if (len > sizeof(sysex_msg_))
        return;
    std::memcpy(sysex_msg_, data, len);
    sysex_msg_len_ = len;
    sysex_ready_ = true;
}

void MidiControl::OnUsbMidiRx(uint8_t *data, size_t size, void *context)
{
    auto *self = static_cast<MidiControl *>(context);
    if (!self)
        return;

    for (size_t i = 0; i < size; i++)
    {
        const uint8_t b = data[i];
        if (!self->in_sysex_)
        {
            if (b == 0xF0)
            {
                self->in_sysex_ = true;
                self->sysex_build_len_ = 0;
                self->sysex_build_[self->sysex_build_len_++] = b;
            }
            continue;
        }

        if (self->sysex_build_len_ < sizeof(self->sysex_build_))
            self->sysex_build_[self->sysex_build_len_++] = b;
        else
        {
            // overflow: drop and resync
            self->in_sysex_ = false;
            self->sysex_build_len_ = 0;
            continue;
        }

        if (b == 0xF7)
        {
            self->QueueSysexIfFree(self->sysex_build_, self->sysex_build_len_);
            self->in_sysex_ = false;
            self->sysex_build_len_ = 0;
        }
    }
}

void MidiControl::SendEffectList()
{
    // Minimal, 7-bit safe effect list: one message per effect.
    // F0 7D 34 <typeId> <nameLen> <name...> F7
    const uint8_t types[] = {
        DelayEffect::TypeId,
        StereoSweepDelayEffect::TypeId,
        DistortionEffect::TypeId,
        StereoMixerEffect::TypeId,
        SimpleReverbEffect::TypeId,
    };

    for (uint8_t typeId : types)
    {
        const EffectMeta *meta = EffectRegistry::Lookup(typeId);
        if (!meta || !meta->name)
            continue;

        const char *name = meta->name;
        size_t nameLen = std::strlen(name);
        if (nameLen > 60)
            nameLen = 60;

        uint8_t msg[96];
        size_t p = 0;
        msg[p++] = 0xF0;
        msg[p++] = 0x7D;
        msg[p++] = 0x34;
        msg[p++] = typeId & 0x7F;
        msg[p++] = (uint8_t)nameLen;
        for (size_t i = 0; i < nameLen; i++)
            msg[p++] = (uint8_t)(name[i] & 0x7F);
        msg[p++] = 0xF7;

        SendSysEx(msg, p);
        if (hw_)
            hw_->DelayMs(1);
    }
}

void MidiControl::SendPatchDump()
{
    // 7-bit-safe patch dump.
    // F0 7D 13 <numSlots>
    //   12x slots: slotIndex typeId enabled inputL inputR sumToMono dry wet policy numParams (id val)x8
    //   2x buttons: slotIndex mode
    // F7
    uint8_t msg[512];
    size_t p = 0;
    msg[p++] = 0xF0;
    msg[p++] = 0x7D;
    msg[p++] = 0x13;
    msg[p++] = (uint8_t)(current_patch_.numSlots & 0x7F);

    for (int i = 0; i < 12; i++)
    {
        const SlotWireDesc &s = current_patch_.slots[i];
        msg[p++] = (uint8_t)(s.slotIndex & 0x7F);
        msg[p++] = (uint8_t)(s.typeId & 0x7F);
        msg[p++] = (uint8_t)(s.enabled & 0x7F);
        msg[p++] = EncodeRoute(s.inputL);
        msg[p++] = EncodeRoute(s.inputR);
        msg[p++] = (uint8_t)(s.sumToMono & 0x7F);
        msg[p++] = (uint8_t)(s.dry & 0x7F);
        msg[p++] = (uint8_t)(s.wet & 0x7F);
        msg[p++] = (uint8_t)(s.channelPolicy & 0x7F);
        msg[p++] = (uint8_t)(s.numParams & 0x7F);
        for (int k = 0; k < 8; k++)
        {
            msg[p++] = (uint8_t)(s.params[k].id & 0x7F);
            msg[p++] = (uint8_t)(s.params[k].value & 0x7F);
        }
    }

    for (int i = 0; i < 2; i++)
    {
        msg[p++] = (uint8_t)(current_patch_.buttons[i].slotIndex == 0xFF
                                 ? 127
                                 : (current_patch_.buttons[i].slotIndex & 0x7F));
        msg[p++] = (uint8_t)((uint8_t)current_patch_.buttons[i].mode & 0x7F);
    }

    msg[p++] = 0xF7;
    SendSysEx(msg, p);
}

void MidiControl::HandleSysexMessage(const uint8_t *bytes, size_t len)
{
    if (len < 4)
        return;
    if (bytes[0] != 0xF0 || bytes[len - 1] != 0xF7)
        return;
    if (bytes[1] != 0x7D)
        return;

    const uint8_t type = bytes[2];
    switch (type)
    {
    case 0x32: // request all effect meta
        SendEffectList();
        break;
    case 0x12: // request patch dump
        SendPatchDump();
        break;
    case 0x20: // set param: F0 7D 20 <slot> <paramId> <value> F7
        if (len >= 7)
        {
            const uint8_t slot = bytes[3] & 0x7F;
            const uint8_t pid = bytes[4] & 0x7F;
            const uint8_t value = bytes[5] & 0x7F;
            if (slot < 12)
            {
                param_slot_ = slot;
                param_id_ = pid;
                param_value_ = value;
                param_pending_ = true;
            }
        }
        break;
    default:
        break;
    }
}

void MidiControl::Process()
{
    if (sysex_ready_)
    {
        sysex_ready_ = false;
        HandleSysexMessage(sysex_msg_, sysex_msg_len_);
    }
}

void MidiControl::ApplyPendingParamInAudioThread()
{
    if (!param_pending_)
        return;

    param_pending_ = false;
    const uint8_t slot = param_slot_;
    if (!board_ || slot >= 12)
        return;

    auto *fx = (*board_).slots[slot].effect;
    if (fx)
        fx->SetParam(param_id_, (float)param_value_ / 127.0f);
}
