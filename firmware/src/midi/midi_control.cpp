#include "midi/midi_control.h"

#include <cstring>

#include "audio/audio_processor.h"
#include "effects/effect_registry.h"
#include "midi/sysex_utils.h"

#include "effects/delay.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/overdrive.h"
#include "effects/stereo_mixer.h"
#include "effects/reverb.h"
#include "effects/compressor.h"
#include "effects/chorus.h"
#include "effects/noise_gate.h"
#include "effects/eq.h"
#include "effects/flanger.h"
#include "effects/phaser.h"
#include "effects/neural_amp.h"
#include "effects/cabinet_ir.h"

namespace
{
    static inline uint32_t PackPending(MidiControl::PendingKind kind, uint8_t slot, uint8_t id, uint8_t value)
    {
        return (uint32_t)kind | ((uint32_t)slot << 8) | ((uint32_t)id << 16) | ((uint32_t)value << 24);
    }

    static inline MidiControl::PendingKind PendingKindOf(uint32_t w)
    {
        return (MidiControl::PendingKind)(w & 0xFF);
    }

    static inline uint8_t PendingSlotOf(uint32_t w) { return (uint8_t)((w >> 8) & 0xFF); }
    static inline uint8_t PendingIdOf(uint32_t w) { return (uint8_t)((w >> 16) & 0xFF); }
    static inline uint8_t PendingValueOf(uint32_t w) { return (uint8_t)((w >> 24) & 0xFF); }

    static inline bool IsLegacyCmd(uint8_t b)
    {
        switch (b)
        {
        case 0x12: // request patch dump
        case 0x20: // set param
        case 0x21: // set enabled
        case 0x22: // set type
        case 0x32: // request meta
            return true;
        default:
            return false;
        }
    }

    static inline bool ExtractParenUnit(const char *s, const char **outPtr, size_t *outLen)
    {
        if (!s)
            return false;
        const char *open = std::strchr(s, '(');
        const char *close = open ? std::strchr(open, ')') : nullptr;
        if (!open || !close || close <= open + 1)
            return false;
        *outPtr = open + 1;
        *outLen = (size_t)(close - (open + 1));
        return true;
    }

    static inline void InferUnitSuffix(uint8_t effectTypeId,
                                       const char *name,
                                       const char *desc,
                                       const char **outPtr,
                                       size_t *outLen)
    {
        *outPtr = "";
        *outLen = 0;

        // Special-case: Graphic EQ bands are gains in dB.
        // (TypeId 18 in core metadata.)
        if (effectTypeId == 18)
        {
            *outPtr = "dB";
            *outLen = 2;
            return;
        }

        // Prefer explicit "(unit)" in name/description.
        if (ExtractParenUnit(name, outPtr, outLen))
            return;
        if (ExtractParenUnit(desc, outPtr, outLen))
            return;

        // Best-effort fallbacks.
        if (name)
        {
            if (std::strstr(name, "kHz"))
            {
                *outPtr = "kHz";
                *outLen = 3;
                return;
            }
            if (std::strstr(name, "Hz"))
            {
                *outPtr = "Hz";
                *outLen = 2;
                return;
            }
            if (std::strstr(name, "ms"))
            {
                *outPtr = "ms";
                *outLen = 2;
                return;
            }
            if (std::strstr(name, "dB"))
            {
                *outPtr = "dB";
                *outLen = 2;
                return;
            }
        }
        if (desc)
        {
            if (std::strstr(desc, "kHz"))
            {
                *outPtr = "kHz";
                *outLen = 3;
                return;
            }
            if (std::strstr(desc, "Hz"))
            {
                *outPtr = "Hz";
                *outLen = 2;
                return;
            }
            if (std::strstr(desc, "ms"))
            {
                *outPtr = "ms";
                *outLen = 2;
                return;
            }
            if (std::strstr(desc, "dB"))
            {
                *outPtr = "dB";
                *outLen = 2;
                return;
            }
        }
    }
}

uint8_t MidiControl::EncodeRoute(uint8_t r)
{
    // SysEx data bytes must be 7-bit.
    // Encode ROUTE_INPUT (255) as 127.
    return (r == ROUTE_INPUT) ? 127 : (uint8_t)(r & 0x7F);
}

void MidiControl::Init(daisy::DaisySeed &hw, AudioProcessor &processor, Board &board, TempoSource &tempo)
{
    hw_ = &hw;
    processor_ = &processor;
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
    // Normalize slotIndex fields (host/UI uses these as stable IDs).
    for (uint8_t i = 0; i < 12; i++)
        current_patch_.slots[i].slotIndex = i;
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
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x41, q[0], q[1], q[2], q[3], q[4], 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendButtonStateChange(uint8_t btn, uint8_t slot, bool enabled)
{
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x40, btn, slot, (uint8_t)(enabled ? 1 : 0), 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendStatusUpdate(float inputLevel, float outputLevel, float cpuAvg, float cpuMax)
{
    // Pack 4 Q16.16 values: inputLevel, outputLevel, cpuAvg, cpuMax
    uint8_t qIn[5], qOut[5], qAvg[5], qMax[5];
    packQ16_16(floatToQ16_16(inputLevel), qIn);
    packQ16_16(floatToQ16_16(outputLevel), qOut);
    packQ16_16(floatToQ16_16(cpuAvg), qAvg);
    packQ16_16(floatToQ16_16(cpuMax), qMax);

    // F0 7D 01 42 [inputLevel 5B] [outputLevel 5B] [cpuAvg 5B] [cpuMax 5B] F7
    uint8_t msg[] = {
        0xF0, 0x7D, 0x01, 0x42,
        qIn[0], qIn[1], qIn[2], qIn[3], qIn[4],
        qOut[0], qOut[1], qOut[2], qOut[3], qOut[4],
        qAvg[0], qAvg[1], qAvg[2], qAvg[3], qAvg[4],
        qMax[0], qMax[1], qMax[2], qMax[3], qMax[4],
        0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendSetParam(uint8_t slot, uint8_t paramId, uint8_t value)
{
    // SysEx: F0 7D 20 <slot> <paramId> <value> F7
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x20, (uint8_t)(slot & 0x7F), (uint8_t)(paramId & 0x7F), (uint8_t)(value & 0x7F), 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendSetEnabled(uint8_t slot, bool enabled)
{
    // SysEx: F0 7D 21 <slot> <enabled> F7
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x21, (uint8_t)(slot & 0x7F), (uint8_t)(enabled ? 1 : 0), 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::SendSetType(uint8_t slot, uint8_t typeId)
{
    // SysEx: F0 7D 22 <slot> <typeId> F7
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x22, (uint8_t)(slot & 0x7F), (uint8_t)(typeId & 0x7F), 0xF7};
    SendSysEx(msg, sizeof(msg));
}

void MidiControl::QueueSysexIfFree(const uint8_t *data, size_t len)
{
    daisy::ScopedIrqBlocker lock;

    // Calculate next head position
    size_t next_head = (sysex_queue_head_ + 1) % kSysexQueueSlots;

    // Check if queue is full (head would catch up to tail)
    if (next_head == sysex_queue_tail_)
        return; // Queue full, drop message

    if (len > kSysexSlotSize)
        return; // Message too large

    // Copy to current head slot
    std::memcpy(sysex_queue_[sysex_queue_head_], data, len);
    sysex_queue_len_[sysex_queue_head_] = len;

    // Advance head
    sysex_queue_head_ = next_head;
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
    // 7-bit safe effect list: one message per effect.
    // Legacy (name only): F0 7D 34 <typeId> <nameLen> <name...> F7
    // V2 (name + params): F0 7D 35 <typeId> <nameLen> <name...> <numParams>
    //    (paramId kind nameLen name...)xN
    // F7
    const uint8_t types[] = {
        DelayEffect::TypeId,
        StereoSweepDelayEffect::TypeId,
        OverdriveEffect::TypeId,
        StereoMixerEffect::TypeId,
        SimpleReverbEffect::TypeId,
        CompressorEffect::TypeId,
        ChorusEffect::TypeId,
        NoiseGateEffect::TypeId,
        GraphicEQEffect::TypeId,
        FlangerEffect::TypeId,
        PhaserEffect::TypeId,
        NeuralAmpEffect::TypeId,
        CabinetIREffect::TypeId,
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

        {
            uint8_t msg[96];
            size_t p = 0;
            msg[p++] = 0xF0;
            msg[p++] = 0x7D;
            msg[p++] = 0x01; // sender (firmware)
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

        // Send V5 metadata with effect/param descriptions + units + optional number ranges.
        {
            uint8_t msg[512];
            size_t p = 0;
            msg[p++] = 0xF0;
            msg[p++] = 0x7D;
            msg[p++] = 0x01; // sender (firmware)
            msg[p++] = 0x38; // EFFECT_META_V5
            msg[p++] = typeId & 0x7F;
            msg[p++] = (uint8_t)nameLen;
            for (size_t i = 0; i < nameLen; i++)
                msg[p++] = (uint8_t)(name[i] & 0x7F);

            // Send 3-character short name (always 3 bytes)
            const char *shortName = meta->shortName ? meta->shortName : "---";
            for (int i = 0; i < 3; i++)
                msg[p++] = (uint8_t)(shortName[i] & 0x7F);

            const char *effectDesc = meta->description ? meta->description : "";
            size_t effectDescLen = std::strlen(effectDesc);
            if (effectDescLen > 80)
                effectDescLen = 80;
            msg[p++] = (uint8_t)(effectDescLen & 0x7F);
            for (size_t i = 0; i < effectDescLen; i++)
                msg[p++] = (uint8_t)(effectDesc[i] & 0x7F);

            const uint8_t numParams = meta->numParams;
            msg[p++] = (uint8_t)(numParams & 0x7F);
            for (uint8_t pi = 0; pi < numParams; pi++)
            {
                const ParamInfo &par = meta->params[pi];
                msg[p++] = (uint8_t)(par.id & 0x7F);
                msg[p++] = (uint8_t)((uint8_t)par.kind & 0x7F);

                uint8_t flags = 0;
                const bool hasNumberRange = (par.kind == ParamValueKind::Number && par.number);
                if (hasNumberRange)
                    flags |= 0x01;
                msg[p++] = (uint8_t)(flags & 0x7F);

                const char *pname = par.name ? par.name : "";
                size_t pnameLen = std::strlen(pname);
                if (pnameLen > 24)
                    pnameLen = 24;
                msg[p++] = (uint8_t)(pnameLen & 0x7F);
                for (size_t j = 0; j < pnameLen; j++)
                    msg[p++] = (uint8_t)(pname[j] & 0x7F);

                const char *pdesc = par.description ? par.description : "";
                size_t pdescLen = std::strlen(pdesc);
                if (pdescLen > 80)
                    pdescLen = 80;
                msg[p++] = (uint8_t)(pdescLen & 0x7F);
                for (size_t j = 0; j < pdescLen; j++)
                    msg[p++] = (uint8_t)(pdesc[j] & 0x7F);

                // Prefix currently unused (kept for future-proofing)
                msg[p++] = 0;

                const char *unitSuffix = "";
                size_t unitSufLen = 0;
                InferUnitSuffix(typeId, pname, pdesc, &unitSuffix, &unitSufLen);
                if (unitSufLen > 16)
                    unitSufLen = 16;
                msg[p++] = (uint8_t)(unitSufLen & 0x7F);
                for (size_t j = 0; j < unitSufLen; j++)
                    msg[p++] = (uint8_t)(unitSuffix[j] & 0x7F);

                if (hasNumberRange)
                {
                    uint8_t q[5];
                    packQ16_16(floatToQ16_16(par.number->minValue), q);
                    for (int k = 0; k < 5; k++)
                        msg[p++] = q[k];
                    packQ16_16(floatToQ16_16(par.number->maxValue), q);
                    for (int k = 0; k < 5; k++)
                        msg[p++] = q[k];
                    packQ16_16(floatToQ16_16(par.number->step), q);
                    for (int k = 0; k < 5; k++)
                        msg[p++] = q[k];
                }
            }

            msg[p++] = 0xF7;
            SendSysEx(msg, p);
        }
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
    msg[p++] = 0x01; // sender (firmware)
    msg[p++] = 0x13;
    msg[p++] = (uint8_t)(current_patch_.numSlots & 0x7F);

    for (int i = 0; i < 12; i++)
    {
        const SlotWireDesc &s = current_patch_.slots[i];
        // Always transmit the array index as slotIndex to guarantee uniqueness.
        msg[p++] = (uint8_t)(i & 0x7F);
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

    uint8_t type = 0;
    size_t payload = 0;
    if (IsLegacyCmd(bytes[2]))
    {
        // Legacy: F0 7D <cmd> ... F7
        type = bytes[2];
        payload = 3;
    }
    else
    {
        // Sender-based: F0 7D <sender> <cmd> ... F7
        if (len < 5)
            return;
        type = bytes[3];
        payload = 4;
    }

    switch (type)
    {
    case 0x32: // request all effect meta
        SendEffectList();
        break;
    case 0x12: // request patch dump
        // Bundle names with patch transfer by sending meta before the patch.
        SendEffectList();
        if (hw_)
            hw_->DelayMs(10); // Small delay to let effect list messages flush
        SendPatchDump();
        break;
    case 0x20: // set param: F0 7D 20 <slot> <paramId> <value> F7
        if (payload + 3 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t pid = bytes[payload + 1] & 0x7F;
            const uint8_t value = bytes[payload + 2] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetParam, slot, pid, value);
            }
        }
        break;
    case 0x21: // set slot enabled: F0 7D 21 <slot> <enabled> F7
        if (payload + 2 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t enabled = bytes[payload + 1] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotEnabled, slot, 0, enabled);
            }
        }
        break;
    case 0x22: // set slot type: F0 7D 22 <slot> <typeId> F7
        if (payload + 2 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t typeId = bytes[payload + 1] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotType, slot, 0, typeId);
            }
        }
        break;

    case 0x23: // set slot routing: F0 7D <sender> 23 <slot> <inputL> <inputR> F7
        if (payload + 3 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t inputL_enc = bytes[payload + 1] & 0x7F;
            const uint8_t inputR_enc = bytes[payload + 2] & 0x7F;
            if (slot < 12)
            {
                // Decode 7-bit route encoding: 127 means ROUTE_INPUT (255).
                const uint8_t inputL = (inputL_enc == 127) ? ROUTE_INPUT : inputL_enc;
                const uint8_t inputR = (inputR_enc == 127) ? ROUTE_INPUT : inputR_enc;
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotRouting, slot, inputL, inputR);
            }
        }
        break;

    case 0x24: // set sum-to-mono: F0 7D <sender> 24 <slot> <sumToMono> F7
        if (payload + 2 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t sum = bytes[payload + 1] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotSumToMono, slot, 0, sum);
            }
        }
        break;

    case 0x25: // set mix: F0 7D <sender> 25 <slot> <dry> <wet> F7
        if (payload + 3 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t dry = bytes[payload + 1] & 0x7F;
            const uint8_t wet = bytes[payload + 2] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotMix, slot, dry, wet);
            }
        }
        break;

    case 0x26: // set channel policy: F0 7D <sender> 26 <slot> <policy> F7
        if (payload + 2 <= len)
        {
            const uint8_t slot = bytes[payload + 0] & 0x7F;
            const uint8_t policy = bytes[payload + 1] & 0x7F;
            if (slot < 12)
            {
                daisy::ScopedIrqBlocker lock;
                pending_word_ = PackPending(PendingKind::SetSlotChannelPolicy, slot, 0, policy);
            }
        }
        break;
    default:
        break;
    }
}

void MidiControl::Process()
{
    // Process all queued SysEx messages
    while (true)
    {
        uint8_t local[kSysexSlotSize];
        size_t local_len = 0;
        {
            daisy::ScopedIrqBlocker lock;
            // Check if queue has messages (tail != head)
            if (sysex_queue_tail_ != sysex_queue_head_)
            {
                local_len = sysex_queue_len_[sysex_queue_tail_];
                if (local_len > sizeof(local))
                    local_len = sizeof(local);
                std::memcpy(local, sysex_queue_[sysex_queue_tail_], local_len);
                // Advance tail
                sysex_queue_tail_ = (sysex_queue_tail_ + 1) % kSysexQueueSlots;
            }
        }
        if (local_len == 0)
            break; // Queue empty
        HandleSysexMessage(local, local_len);
    }

    bool sendPatch = false;
    {
        daisy::ScopedIrqBlocker lock;
        if (tx_patch_dump_pending_)
        {
            tx_patch_dump_pending_ = false;
            sendPatch = true;
        }
    }
    if (sendPatch)
        SendPatchDump();
}

void MidiControl::ApplyPendingInAudioThread()
{
    if (!board_)
        return;

    const uint32_t w = pending_word_;
    if (w == 0)
        return;
    pending_word_ = 0;

    const PendingKind kind = PendingKindOf(w);
    const uint8_t slot = PendingSlotOf(w);
    const uint8_t pid = PendingIdOf(w);
    const uint8_t value = PendingValueOf(w);
    if (kind == PendingKind::None || slot >= 12)
        return;

    if (kind == PendingKind::SetParam)
    {
        auto *fx = (*board_).slots[slot].effect;
        if (fx)
            fx->SetParam(pid, (float)value / 127.0f);

        // Keep patch dump in sync.
        if (slot < current_patch_.numSlots)
        {
            SlotWireDesc &sw = current_patch_.slots[slot];
            bool found = false;
            for (uint8_t i = 0; i < sw.numParams && i < 8; i++)
            {
                if (sw.params[i].id == pid)
                {
                    sw.params[i].value = value;
                    found = true;
                    break;
                }
            }
            if (!found && sw.numParams < 8)
            {
                sw.params[sw.numParams++] = {pid, value};
            }
        }
        // Queue individual param change for TX (no echo - this came from external)
        // Note: We don't send back since this was received via SysEx
    }
    else if (kind == PendingKind::SetSlotEnabled)
    {
        const bool en = value != 0;
        (*board_).slots[slot].enabled = en;
        if (slot < current_patch_.numSlots)
            current_patch_.slots[slot].enabled = en ? 1 : 0;
        // Note: We don't send back since this was received via SysEx
    }
    else if (kind == PendingKind::SetSlotType)
    {
        if (!processor_)
            return;

        const uint8_t typeId = value;
        // Update the patch and re-apply. Preserve existing routing.
        if (slot >= current_patch_.numSlots)
        {
            // Extend patch if needed.
            current_patch_.numSlots = slot + 1;
            // Default routing: slot 0 reads from hardware input, others read from previous slot.
            uint8_t inputRoute = (slot == 0) ? ROUTE_INPUT : (slot - 1);
            current_patch_.slots[slot] = {slot, 0, 1, inputRoute, inputRoute, 0, 0, 127, (uint8_t)ChannelPolicy::Auto, 0, {}};
        }
        // Ensure slotIndex is always correct even for pre-existing slots.
        current_patch_.slots[slot].slotIndex = slot;
        current_patch_.slots[slot].typeId = typeId;
        current_patch_.slots[slot].enabled = 1;
        current_patch_.slots[slot].numParams = 0;

        // If this is a new effect in slot > 0, make sure it chains from the previous slot.
        if (slot > 0 && current_patch_.slots[slot].inputL == ROUTE_INPUT)
        {
            current_patch_.slots[slot].inputL = slot - 1;
            current_patch_.slots[slot].inputR = slot - 1;
        }

        // Copy and apply in-place.
        pending_patch_ = current_patch_;
        processor_->ApplyPatch(pending_patch_);

        // Refresh the patch's param snapshot based on the newly-instantiated effect.
        auto *fx = (*board_).slots[slot].effect;
        if (fx)
        {
            ParamDesc snap[8];
            const uint8_t n = fx->GetParamsSnapshot(snap, 8);
            SlotWireDesc &sw = current_patch_.slots[slot];
            sw.numParams = n;
            for (uint8_t i = 0; i < 8; i++)
            {
                sw.params[i].id = (i < n) ? snap[i].id : 0;
                sw.params[i].value = (i < n) ? snap[i].value : 0;
            }
        }
        // Note: We don't send back since this was received via SysEx
    }
    else if (kind == PendingKind::SetSlotRouting)
    {
        if (!processor_)
            return;

        if (slot >= current_patch_.numSlots)
            current_patch_.numSlots = slot + 1;

        current_patch_.slots[slot].slotIndex = slot;
        current_patch_.slots[slot].inputL = pid;
        current_patch_.slots[slot].inputR = value;

        pending_patch_ = current_patch_;
        processor_->ApplyPatch(pending_patch_);
    }
    else if (kind == PendingKind::SetSlotSumToMono)
    {
        if (!processor_)
            return;

        if (slot >= current_patch_.numSlots)
            current_patch_.numSlots = slot + 1;

        current_patch_.slots[slot].slotIndex = slot;
        current_patch_.slots[slot].sumToMono = (value != 0) ? 1 : 0;

        pending_patch_ = current_patch_;
        processor_->ApplyPatch(pending_patch_);
    }
    else if (kind == PendingKind::SetSlotMix)
    {
        if (!processor_)
            return;

        if (slot >= current_patch_.numSlots)
            current_patch_.numSlots = slot + 1;

        current_patch_.slots[slot].slotIndex = slot;
        current_patch_.slots[slot].dry = pid;
        current_patch_.slots[slot].wet = value;

        pending_patch_ = current_patch_;
        processor_->ApplyPatch(pending_patch_);
    }
    else if (kind == PendingKind::SetSlotChannelPolicy)
    {
        if (!processor_)
            return;

        if (slot >= current_patch_.numSlots)
            current_patch_.numSlots = slot + 1;

        current_patch_.slots[slot].slotIndex = slot;
        current_patch_.slots[slot].channelPolicy = value;

        pending_patch_ = current_patch_;
        processor_->ApplyPatch(pending_patch_);
    }
}
