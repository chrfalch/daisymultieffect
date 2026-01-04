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
            constexpr uint8_t SET_PARAM = 0x20;
            constexpr uint8_t SET_ENABLED = 0x21;
            constexpr uint8_t SET_TYPE = 0x22;
            constexpr uint8_t REQUEST_META = 0x32;
        }

        // Response codes
        namespace Resp
        {
            constexpr uint8_t PATCH_DUMP = 0x13;
            constexpr uint8_t EFFECT_META = 0x33;
            constexpr uint8_t EFFECT_META_V3 = 0x36;
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
            bool valid = false;
        };

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
         * Format: F0 7D <sender> 13 <numSlots> [slot data...] [button data...] F7
         */
        inline std::vector<uint8_t> encodePatchDump(uint8_t sender, const PatchWireDesc &patch)
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

            sysex.push_back(0xF7);
            return sysex;
        }

        //=========================================================================
        // Decoder - Parse SysEx messages
        // Note: JUCE strips F0/F7, so data starts with manufacturer ID
        //=========================================================================

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

            case Cmd::REQUEST_PATCH:
            case Cmd::REQUEST_META:
                msg.valid = true;
                break;

            default:
                break;
            }

            return msg;
        }

        /**
         * Decode a full patch dump.
         * Input format (with F0/F7 stripped): 7D 13 <numSlots> [slot data...] [button data...]
         */
        inline bool decodePatchDump(const uint8_t *data, int size, PatchWireDesc &outPatch)
        {
            if (size < 4)
                return false;
            if (data[0] != MANUFACTURER_ID)
                return false;
            if (data[1] != Resp::PATCH_DUMP)
                return false;

            outPatch = {};
            outPatch.numSlots = data[2];

            int offset = 3;

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
            // offset += 4;

            return true;
        }

    } // namespace MidiProtocol
} // namespace daisyfx
