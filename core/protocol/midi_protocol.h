#pragma once

#include "../patch/patch_protocol.h"
#include <vector>
#include <cstdint>
#include <cstring>

namespace daisyfx
{

    /**
     * MIDI Protocol constants and encoder/decoder.
     *
     * This is a pure data transformation layer - no state, no side effects.
     * Messages use format: F0 7D <sender> <cmd> <data...> F7
     *
     * The sender ID allows receivers to ignore their own loopback messages
     * when using IAC Driver or similar MIDI routing.
     */
    namespace MidiProtocol
    {

        // Manufacturer ID (educational/development)
        constexpr uint8_t MANUFACTURER_ID = 0x7D;

        // Sender IDs - each client type has a unique ID
        namespace Sender
        {
            constexpr uint8_t FIRMWARE = 0x01;
            constexpr uint8_t VST = 0x02;
            constexpr uint8_t SWIFT = 0x03;
            constexpr uint8_t UNKNOWN = 0x00;
        }

        // Command codes (requests/actions)
        namespace Cmd
        {
            constexpr uint8_t REQUEST_PATCH = 0x12;
            constexpr uint8_t LOAD_PATCH = 0x14;
            constexpr uint8_t SET_PARAM = 0x20;
            constexpr uint8_t SET_ENABLED = 0x21;
            constexpr uint8_t SET_TYPE = 0x22;
            constexpr uint8_t SET_ROUTING = 0x23;
            constexpr uint8_t SET_SUM_TO_MONO = 0x24;
            constexpr uint8_t SET_MIX = 0x25;
            constexpr uint8_t SET_CHANNEL_POLICY = 0x26;
            constexpr uint8_t SET_INPUT_GAIN = 0x27;
            constexpr uint8_t SET_OUTPUT_GAIN = 0x28;
            constexpr uint8_t REQUEST_META = 0x32;
        }

        // Response codes
        namespace Resp
        {
            constexpr uint8_t PATCH_DUMP = 0x13;
            constexpr uint8_t EFFECT_META = 0x33;
            constexpr uint8_t EFFECT_META_V3 = 0x36;
            constexpr uint8_t EFFECT_META_V4 = 0x37;
            constexpr uint8_t EFFECT_META_V5 = 0x38;
            constexpr uint8_t STATUS_UPDATE = 0x42;
        }

        // Effect type IDs
        namespace EffectType
        {
            constexpr uint8_t OFF = 0;
            constexpr uint8_t DELAY = 1;
            constexpr uint8_t DISTORTION = 10;
            constexpr uint8_t SWEEP_DELAY = 12;
            constexpr uint8_t MIXER = 13;
            constexpr uint8_t REVERB = 14;
            constexpr uint8_t COMPRESSOR = 15;
            constexpr uint8_t CHORUS = 16;
        }

        //=========================================================================
        // Q16.16 Fixed-Point Encoding (for float values over SysEx)
        //=========================================================================

        inline int32_t floatToQ16_16(float v) { return static_cast<int32_t>(v * 65536.0f + 0.5f); }

        inline void packQ16_16(int32_t value, uint8_t out[5])
        {
            uint32_t u = static_cast<uint32_t>(value);
            out[0] = u & 0x7F;
            out[1] = (u >> 7) & 0x7F;
            out[2] = (u >> 14) & 0x7F;
            out[3] = (u >> 21) & 0x7F;
            out[4] = (u >> 28) & 0x7F;
        }

        inline float unpackQ16_16(const uint8_t in[5])
        {
            uint32_t u = static_cast<uint32_t>(in[0]) |
                         (static_cast<uint32_t>(in[1]) << 7) |
                         (static_cast<uint32_t>(in[2]) << 14) |
                         (static_cast<uint32_t>(in[3]) << 21) |
                         (static_cast<uint32_t>(in[4]) << 28);
            return static_cast<float>(static_cast<int32_t>(u)) / 65536.0f;
        }

        //=========================================================================
        // Decoded Command Structure
        //=========================================================================

        struct DecodedMessage
        {
            uint8_t sender = 0; // Who sent this message
            uint8_t command = 0;
            uint8_t slot = 0;
            uint8_t paramId = 0;
            uint8_t value = 0;
            bool enabled = false;
            uint8_t typeId = 0;
            uint8_t inputL = 0;
            uint8_t inputR = 0;
            bool sumToMono = false;
            uint8_t dry = 0;
            uint8_t wet = 0;
            uint8_t channelPolicy = 0;
            float inputGainDb = 0.0f;  // For SET_INPUT_GAIN
            float outputGainDb = 0.0f; // For SET_OUTPUT_GAIN
            PatchWireDesc patch{};     // For LOAD_PATCH
            bool valid = false;
        };

        inline uint8_t decodeRouteByte(uint8_t encoded)
        {
            // On-wire 7-bit encoding uses 127 to represent ROUTE_INPUT (255).
            return (encoded == 127) ? 255 : encoded;
        }

        //=========================================================================
        // Encoders - Create SysEx messages (all include sender ID)
        //=========================================================================

        inline std::vector<uint8_t> encodeSetEnabled(uint8_t sender, uint8_t slot, bool enabled)
        {
            return {0xF0, MANUFACTURER_ID, sender, Cmd::SET_ENABLED,
                    static_cast<uint8_t>(slot & 0x7F),
                    static_cast<uint8_t>(enabled ? 1 : 0),
                    0xF7};
        }

        inline std::vector<uint8_t> encodeSetType(uint8_t sender, uint8_t slot, uint8_t typeId)
        {
            return {0xF0, MANUFACTURER_ID, sender, Cmd::SET_TYPE,
                    static_cast<uint8_t>(slot & 0x7F),
                    static_cast<uint8_t>(typeId & 0x7F),
                    0xF7};
        }

        inline std::vector<uint8_t> encodeSetParam(uint8_t sender, uint8_t slot, uint8_t paramId, uint8_t value)
        {
            return {0xF0, MANUFACTURER_ID, sender, Cmd::SET_PARAM,
                    static_cast<uint8_t>(slot & 0x7F),
                    static_cast<uint8_t>(paramId & 0x7F),
                    static_cast<uint8_t>(value & 0x7F),
                    0xF7};
        }

        inline std::vector<uint8_t> encodeRequestPatch(uint8_t sender)
        {
            return {0xF0, MANUFACTURER_ID, sender, Cmd::REQUEST_PATCH, 0xF7};
        }

        inline std::vector<uint8_t> encodeRequestMeta(uint8_t sender)
        {
            return {0xF0, MANUFACTURER_ID, sender, Cmd::REQUEST_META, 0xF7};
        }

        /**
         * Encode a full patch dump response.
         * Format: F0 7D <sender> 13 <numSlots> [slot data...] [button data...] [inputGainDb] [outputGainDb] F7
         */
        inline std::vector<uint8_t> encodePatchDump(uint8_t sender, const PatchWireDesc &patch,
                                                    float inputGainDb = 18.0f, float outputGainDb = 0.0f)
        {
            std::vector<uint8_t> sysex;
            sysex.reserve(512);

            sysex.push_back(0xF0);
            sysex.push_back(MANUFACTURER_ID);
            sysex.push_back(sender);
            sysex.push_back(Resp::PATCH_DUMP);
            sysex.push_back(patch.numSlots & 0x7F);

            // 12 slots
            for (uint8_t i = 0; i < 12; ++i)
            {
                const auto &slot = patch.slots[i];
                sysex.push_back(i & 0x7F);
                sysex.push_back(slot.typeId & 0x7F);
                sysex.push_back(slot.enabled & 0x7F);
                sysex.push_back(slot.inputL & 0x7F);
                sysex.push_back(slot.inputR & 0x7F);
                sysex.push_back(slot.sumToMono & 0x7F);
                sysex.push_back(slot.dry & 0x7F);
                sysex.push_back(slot.wet & 0x7F);
                sysex.push_back(slot.channelPolicy & 0x7F);
                sysex.push_back(slot.numParams & 0x7F);

                // 8 param pairs
                for (uint8_t p = 0; p < 8; ++p)
                {
                    sysex.push_back(slot.params[p].id & 0x7F);
                    sysex.push_back(slot.params[p].value & 0x7F);
                }
            }

            // 2 button mappings
            for (int b = 0; b < 2; ++b)
            {
                sysex.push_back(127); // unassigned
                sysex.push_back(0);
            }

            // Global gain settings (Q16.16 encoded, 5 bytes each)
            uint8_t inGain[5], outGain[5];
            packQ16_16(floatToQ16_16(inputGainDb), inGain);
            packQ16_16(floatToQ16_16(outputGainDb), outGain);
            for (int i = 0; i < 5; ++i)
                sysex.push_back(inGain[i]);
            for (int i = 0; i < 5; ++i)
                sysex.push_back(outGain[i]);

            sysex.push_back(0xF7);
            return sysex;
        }

        /**
         * Encode LOAD_PATCH command.
         * Format: F0 7D <sender> 14 <numSlots> [slot data...] [button data...] [inputGainDb] [outputGainDb] F7
         * Same wire format as PATCH_DUMP but with command code 0x14.
         */
        inline std::vector<uint8_t> encodeLoadPatch(uint8_t sender, const PatchWireDesc &patch,
                                                    float inputGainDb = 18.0f, float outputGainDb = 0.0f)
        {
            std::vector<uint8_t> sysex;
            sysex.reserve(512);

            sysex.push_back(0xF0);
            sysex.push_back(MANUFACTURER_ID);
            sysex.push_back(sender);
            sysex.push_back(Cmd::LOAD_PATCH);
            sysex.push_back(patch.numSlots & 0x7F);

            // 12 slots
            for (uint8_t i = 0; i < 12; ++i)
            {
                const auto &slot = patch.slots[i];
                sysex.push_back(i & 0x7F);
                sysex.push_back(slot.typeId & 0x7F);
                sysex.push_back(slot.enabled & 0x7F);
                sysex.push_back(slot.inputL & 0x7F);
                sysex.push_back(slot.inputR & 0x7F);
                sysex.push_back(slot.sumToMono & 0x7F);
                sysex.push_back(slot.dry & 0x7F);
                sysex.push_back(slot.wet & 0x7F);
                sysex.push_back(slot.channelPolicy & 0x7F);
                sysex.push_back(slot.numParams & 0x7F);

                // 8 param pairs
                for (uint8_t p = 0; p < 8; ++p)
                {
                    sysex.push_back(slot.params[p].id & 0x7F);
                    sysex.push_back(slot.params[p].value & 0x7F);
                }
            }

            // 2 button mappings
            for (int b = 0; b < 2; ++b)
            {
                sysex.push_back(127); // unassigned
                sysex.push_back(0);
            }

            // Global gain settings (Q16.16 encoded, 5 bytes each)
            uint8_t inGain[5], outGain[5];
            packQ16_16(floatToQ16_16(inputGainDb), inGain);
            packQ16_16(floatToQ16_16(outputGainDb), outGain);
            for (int i = 0; i < 5; ++i)
                sysex.push_back(inGain[i]);
            for (int i = 0; i < 5; ++i)
                sysex.push_back(outGain[i]);

            sysex.push_back(0xF7);
            return sysex;
        }

        /**
         * Encode SET_INPUT_GAIN command.
         * Format: F0 7D <sender> 27 <gainDb_Q16.16_5bytes> F7
         * @param gainDb Input gain in dB (0 to +24 dB typical)
         */
        inline std::vector<uint8_t> encodeSetInputGain(uint8_t sender, float gainDb)
        {
            uint8_t q[5];
            packQ16_16(floatToQ16_16(gainDb), q);
            return {0xF0, MANUFACTURER_ID, sender, Cmd::SET_INPUT_GAIN,
                    q[0], q[1], q[2], q[3], q[4], 0xF7};
        }

        /**
         * Encode SET_OUTPUT_GAIN command.
         * Format: F0 7D <sender> 28 <gainDb_Q16.16_5bytes> F7
         * @param gainDb Output gain in dB (-12 to +12 dB typical)
         */
        inline std::vector<uint8_t> encodeSetOutputGain(uint8_t sender, float gainDb)
        {
            uint8_t q[5];
            packQ16_16(floatToQ16_16(gainDb), q);
            return {0xF0, MANUFACTURER_ID, sender, Cmd::SET_OUTPUT_GAIN,
                    q[0], q[1], q[2], q[3], q[4], 0xF7};
        }

        /**
         * Encode STATUS_UPDATE response.
         * Format: F0 7D <sender> 42 <inputLevel_Q16.16_5bytes> <outputLevel_Q16.16_5bytes>
         *         <cpuAvg_Q16.16_5bytes> <cpuMax_Q16.16_5bytes> F7
         *
         * All values are linear (0.0-1.0+ range).
         * @param inputLevel Peak input level (linear, 0.0-1.0+)
         * @param outputLevel Peak output level (linear, 0.0-1.0+)
         * @param cpuAvg Average CPU load (0.0-1.0)
         * @param cpuMax Maximum CPU load since last reset (0.0-1.0)
         */
        inline std::vector<uint8_t> encodeStatusUpdate(uint8_t sender,
                                                       float inputLevel,
                                                       float outputLevel,
                                                       float cpuAvg,
                                                       float cpuMax)
        {
            uint8_t qIn[5], qOut[5], qAvg[5], qMax[5];
            packQ16_16(floatToQ16_16(inputLevel), qIn);
            packQ16_16(floatToQ16_16(outputLevel), qOut);
            packQ16_16(floatToQ16_16(cpuAvg), qAvg);
            packQ16_16(floatToQ16_16(cpuMax), qMax);

            return {0xF0, MANUFACTURER_ID, sender, Resp::STATUS_UPDATE,
                    qIn[0], qIn[1], qIn[2], qIn[3], qIn[4],
                    qOut[0], qOut[1], qOut[2], qOut[3], qOut[4],
                    qAvg[0], qAvg[1], qAvg[2], qAvg[3], qAvg[4],
                    qMax[0], qMax[1], qMax[2], qMax[3], qMax[4],
                    0xF7};
        }

        //=========================================================================
        // Decoder - Parse SysEx messages
        // Note: JUCE strips F0/F7, so data starts with manufacturer ID
        //=========================================================================

        // Forward declaration for use in decode()
        inline bool decodePatchDump(const uint8_t *data, int size, PatchWireDesc &outPatch,
                                    float *outInputGainDb = nullptr, float *outOutputGainDb = nullptr);

        /**
         * Decode a SysEx message (with F0/F7 already stripped by JUCE).
         * Input format: 7D <sender> <cmd> <data...>
         */
        inline DecodedMessage decode(const uint8_t *data, int size)
        {
            DecodedMessage msg;

            if (size < 3)
                return msg; // Need at least manufacturer + sender + cmd
            if (data[0] != MANUFACTURER_ID)
                return msg;

            msg.sender = data[1];
            msg.command = data[2];

            switch (msg.command)
            {
            case Cmd::SET_ENABLED:
                if (size >= 5)
                {
                    msg.slot = data[3];
                    msg.enabled = data[4] != 0;
                    msg.valid = true;
                }
                break;

            case Cmd::SET_TYPE:
                if (size >= 5)
                {
                    msg.slot = data[3];
                    msg.typeId = data[4];
                    msg.valid = true;
                }
                break;

            case Cmd::SET_PARAM:
                if (size >= 6)
                {
                    msg.slot = data[3];
                    msg.paramId = data[4];
                    msg.value = data[5];
                    msg.valid = true;
                }
                break;

            case Cmd::SET_ROUTING:
                // 7D <sender> 23 <slot> <inputL> <inputR>
                if (size >= 6)
                {
                    msg.slot = data[3];
                    msg.inputL = decodeRouteByte(data[4]);
                    msg.inputR = decodeRouteByte(data[5]);
                    msg.valid = true;
                }
                break;

            case Cmd::SET_SUM_TO_MONO:
                // 7D <sender> 24 <slot> <sumToMono>
                if (size >= 5)
                {
                    msg.slot = data[3];
                    msg.sumToMono = data[4] != 0;
                    msg.valid = true;
                }
                break;

            case Cmd::SET_MIX:
                // 7D <sender> 25 <slot> <dry> <wet>
                if (size >= 6)
                {
                    msg.slot = data[3];
                    msg.dry = data[4];
                    msg.wet = data[5];
                    msg.valid = true;
                }
                break;

            case Cmd::SET_CHANNEL_POLICY:
                // 7D <sender> 26 <slot> <policy>
                if (size >= 5)
                {
                    msg.slot = data[3];
                    msg.channelPolicy = data[4];
                    msg.valid = true;
                }
                break;

            case Cmd::SET_INPUT_GAIN:
                // 7D <sender> 27 <gainDb_Q16.16_5bytes>
                if (size >= 8)
                {
                    msg.inputGainDb = unpackQ16_16(&data[3]);
                    msg.valid = true;
                }
                break;

            case Cmd::SET_OUTPUT_GAIN:
                // 7D <sender> 28 <gainDb_Q16.16_5bytes>
                if (size >= 8)
                {
                    msg.outputGainDb = unpackQ16_16(&data[3]);
                    msg.valid = true;
                }
                break;

            case Cmd::REQUEST_PATCH:
            case Cmd::REQUEST_META:
                msg.valid = true;
                break;

            case Cmd::LOAD_PATCH:
                // 7D <sender> 14 <numSlots> [slot data...] [button data...] [gains...]
                // Same format as PATCH_DUMP but as a command
                if (size >= 4)
                {
                    // Use the existing decodePatchDump logic by adjusting the pointer
                    // decodePatchDump expects: 7D <sender> 13 <numSlots> ...
                    // We have: 7D <sender> 14 <numSlots> ...
                    // Just decode in-place since the payload is the same
                    if (decodePatchDump(data, size, msg.patch, &msg.inputGainDb, &msg.outputGainDb))
                    {
                        msg.valid = true;
                    }
                }
                break;

            default:
                break;
            }

            return msg;
        }

        /**
         * Decode a full patch dump or load patch command.
         * Input format (with F0/F7 stripped): 7D <sender> <cmd> <numSlots> [slot data...] [button data...]
         * Accepts both PATCH_DUMP (0x13) and LOAD_PATCH (0x14) commands.
         */
        inline bool decodePatchDump(const uint8_t *data, int size, PatchWireDesc &outPatch,
                                    float *outInputGainDb, float *outOutputGainDb)
        {
            if (size < 4)
                return false;
            if (data[0] != MANUFACTURER_ID)
                return false;
            // Accept both PATCH_DUMP response (0x13) and LOAD_PATCH command (0x14)
            // Note: data[1] is sender, data[2] is command in sender-based format
            // For legacy format: data[1] is command
            // Check if data[2] is 0x13 or 0x14 (sender-based format)
            // Or if data[1] is 0x13 (legacy format, though LOAD_PATCH doesn't use legacy)
            uint8_t cmd = data[2];
            int offset = 3;
            if (data[1] == Resp::PATCH_DUMP)
            {
                // Legacy format: 7D 13 <numSlots> ...
                cmd = data[1];
                offset = 2;
            }
            else if (cmd != Resp::PATCH_DUMP && cmd != Cmd::LOAD_PATCH)
            {
                return false;
            }

            outPatch = {};
            outPatch.numSlots = data[offset++];

            // 12 slots, each: slotIndex typeId enabled inputL inputR sumToMono dry wet policy numParams (id val)x8
            // = 10 + 16 = 26 bytes per slot
            for (uint8_t i = 0; i < 12; ++i)
            {
                if (offset + 26 > size)
                    return false;

                auto &slot = outPatch.slots[i];
                slot.slotIndex = data[offset++];
                slot.typeId = data[offset++];
                slot.enabled = data[offset++];
                slot.inputL = data[offset++];
                slot.inputR = data[offset++];
                slot.sumToMono = data[offset++];
                slot.dry = data[offset++];
                slot.wet = data[offset++];
                slot.channelPolicy = data[offset++];
                slot.numParams = data[offset++];

                for (uint8_t p = 0; p < 8; ++p)
                {
                    slot.params[p].id = data[offset++];
                    slot.params[p].value = data[offset++];
                }
            }

            // Skip button data (4 bytes)
            offset += 4;

            // Read global gain settings if present (10 bytes: 5 for input, 5 for output)
            if (offset + 10 <= size)
            {
                if (outInputGainDb)
                    *outInputGainDb = unpackQ16_16(&data[offset]);
                offset += 5;
                if (outOutputGainDb)
                    *outOutputGainDb = unpackQ16_16(&data[offset]);
                offset += 5;
            }
            else
            {
                // Default values if not present (backwards compatibility)
                if (outInputGainDb)
                    *outInputGainDb = 18.0f;
                if (outOutputGainDb)
                    *outOutputGainDb = 0.0f;
            }

            return true;
        }

    } // namespace MidiProtocol
} // namespace daisyfx
