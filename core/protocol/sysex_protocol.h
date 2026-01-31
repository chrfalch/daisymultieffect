/**
 * @file sysex_protocol.h
 * @brief C++ wrapper for SysEx protocol definitions
 *
 * This header wraps the C-compatible sysex_protocol_c.h with C++ namespaces
 * and type-safe enums. The actual values come from the C header to ensure
 * consistency across C, C++, and Swift (via bridging header).
 *
 * All messages use manufacturer ID 0x7D (educational/development use).
 */

#pragma once

#include "sysex_protocol_c.h"
#include <cstdint>

namespace DaisyMultiFX
{
    namespace Protocol
    {

        // =============================================================================
        // Constants (from C header)
        // =============================================================================

        static constexpr uint8_t MANUFACTURER_ID = SYSEX_MANUFACTURER_ID;
        static constexpr uint8_t MAX_SLOTS = SYSEX_MAX_SLOTS;
        static constexpr uint8_t MAX_PARAMS_PER_SLOT = SYSEX_MAX_PARAMS_PER_SLOT;
        static constexpr uint8_t NUM_BUTTONS = SYSEX_NUM_BUTTONS;
        static constexpr uint8_t ROUTE_INPUT = SYSEX_ROUTE_INPUT;

        // =============================================================================
        // Message Types - Host to Device (Commands)
        // =============================================================================
        namespace Command
        {
            static constexpr uint8_t REQUEST_PATCH = SYSEX_CMD_REQUEST_PATCH;
            static constexpr uint8_t LOAD_PATCH = SYSEX_CMD_LOAD_PATCH;
            static constexpr uint8_t SET_PARAM = SYSEX_CMD_SET_PARAM;
            static constexpr uint8_t SET_ENABLED = SYSEX_CMD_SET_ENABLED;
            static constexpr uint8_t SET_TYPE = SYSEX_CMD_SET_TYPE;
            static constexpr uint8_t SET_ROUTING = SYSEX_CMD_SET_ROUTING;
            static constexpr uint8_t SET_SUM_TO_MONO = SYSEX_CMD_SET_SUM_TO_MONO;
            static constexpr uint8_t SET_MIX = SYSEX_CMD_SET_MIX;
            static constexpr uint8_t SET_CHANNEL_POLICY = SYSEX_CMD_SET_CHANNEL_POLICY;
            static constexpr uint8_t SET_INPUT_GAIN = SYSEX_CMD_SET_INPUT_GAIN;
            static constexpr uint8_t SET_OUTPUT_GAIN = SYSEX_CMD_SET_OUTPUT_GAIN;
            static constexpr uint8_t SET_GLOBAL_BYPASS = SYSEX_CMD_SET_GLOBAL_BYPASS;
            static constexpr uint8_t REQUEST_EFFECT_META = SYSEX_CMD_REQUEST_EFFECT_META;
        }

        // =============================================================================
        // Message Types - Device to Host (Responses)
        // =============================================================================
        namespace Response
        {
            static constexpr uint8_t PATCH_DUMP = SYSEX_RESP_PATCH_DUMP;
            static constexpr uint8_t EFFECT_DISCOVERED = SYSEX_RESP_EFFECT_DISCOVERED;
            static constexpr uint8_t EFFECT_META_V2 = SYSEX_RESP_EFFECT_META_V2;
            static constexpr uint8_t EFFECT_META_V3 = SYSEX_RESP_EFFECT_META_V3;
            static constexpr uint8_t EFFECT_META_V4 = SYSEX_RESP_EFFECT_META_V4;
            static constexpr uint8_t EFFECT_META_V5 = SYSEX_RESP_EFFECT_META_V5;
            static constexpr uint8_t EFFECT_META_LIST = SYSEX_RESP_EFFECT_META_LIST;
            static constexpr uint8_t BUTTON_STATE = SYSEX_RESP_BUTTON_STATE;
            static constexpr uint8_t TEMPO_UPDATE = SYSEX_RESP_TEMPO_UPDATE;
            static constexpr uint8_t STATUS_UPDATE = SYSEX_RESP_STATUS_UPDATE;
        }

        // =============================================================================
        // Effect Type IDs
        // =============================================================================
        namespace EffectType
        {
            static constexpr uint8_t OFF = SYSEX_EFFECT_OFF;
            static constexpr uint8_t DELAY = SYSEX_EFFECT_DELAY;
            static constexpr uint8_t DISTORTION = SYSEX_EFFECT_DISTORTION;
            static constexpr uint8_t SWEEP_DELAY = SYSEX_EFFECT_SWEEP_DELAY;
            static constexpr uint8_t MIXER = SYSEX_EFFECT_MIXER;
            static constexpr uint8_t REVERB = SYSEX_EFFECT_REVERB;
            static constexpr uint8_t COMPRESSOR = SYSEX_EFFECT_COMPRESSOR;
            static constexpr uint8_t CHORUS = SYSEX_EFFECT_CHORUS;
        }

        // =============================================================================
        // Enums (C++ type-safe versions)
        // =============================================================================

        /// Channel routing policy for stereo effects
        enum class ChannelPolicy : uint8_t
        {
            Auto = SYSEX_CHANNEL_POLICY_AUTO,
            ForceMono = SYSEX_CHANNEL_POLICY_FORCE_MONO,
            ForceStereo = SYSEX_CHANNEL_POLICY_FORCE_STEREO
        };

        /// Hardware button mode assignment
        enum class ButtonMode : uint8_t
        {
            Unused = SYSEX_BUTTON_MODE_UNUSED,
            ToggleBypass = SYSEX_BUTTON_MODE_TOGGLE_BYPASS,
            TapTempo = SYSEX_BUTTON_MODE_TAP_TEMPO
        };

        // =============================================================================
        // Wire Structures (C++ versions with same layout as C structs)
        // =============================================================================

        /// Button assignment for hardware buttons
        struct ButtonAssignWire
        {
            uint8_t slotIndex; ///< 0-11 or 127 for unassigned
            ButtonMode mode;
        };

        /// Single parameter (id + value pair)
        struct SlotParamWire
        {
            uint8_t id;    ///< Parameter ID within effect
            uint8_t value; ///< 0..127
        };

        /// Complete slot description
        struct SlotWireDesc
        {
            uint8_t slotIndex; ///< 0-11
            uint8_t typeId;    ///< Effect type (see EffectType namespace)
            uint8_t enabled;   ///< 0/1

            uint8_t inputL; ///< Left input route (0-11 = slot, ROUTE_INPUT = hw input)
            uint8_t inputR; ///< Right input route

            uint8_t sumToMono; ///< Sum inputs to mono (0/1)
            uint8_t dry;       ///< Dry level 0..127
            uint8_t wet;       ///< Wet level 0..127

            uint8_t channelPolicy; ///< ChannelPolicy enum value
            uint8_t numParams;     ///< Number of valid params (<= MAX_PARAMS_PER_SLOT)
            SlotParamWire params[MAX_PARAMS_PER_SLOT];
        };

        /// Complete patch description
        struct PatchWireDesc
        {
            uint8_t numSlots; ///< Number of active slots (typically 4 or 12)
            SlotWireDesc slots[MAX_SLOTS];
            ButtonAssignWire buttons[NUM_BUTTONS];
        };

        // =============================================================================
        // Utility Functions
        // =============================================================================

        /// Encode a route value for 7-bit safe transmission
        inline uint8_t encodeRoute(uint8_t route)
        {
            return sysex_encode_route(route);
        }

        /// Decode a route value from 7-bit safe transmission
        inline uint8_t decodeRoute(uint8_t encoded)
        {
            return sysex_decode_route(encoded);
        }

        /// Convert float to Q16.16 fixed point
        inline int32_t floatToQ16_16(float v)
        {
            return sysex_float_to_q16_16(v);
        }

        /// Convert Q16.16 fixed point to float
        inline float q16_16ToFloat(int32_t v)
        {
            return sysex_q16_16_to_float(v);
        }

        /// Pack Q16.16 value into 5 bytes (7-bit safe)
        inline void packQ16_16(int32_t value, uint8_t out[5])
        {
            sysex_pack_q16_16(value, out);
        }

        /// Unpack 5 bytes into Q16.16 value
        inline int32_t unpackQ16_16(const uint8_t in[5])
        {
            return sysex_unpack_q16_16(in);
        }

    } // namespace Protocol
} // namespace DaisyMultiFX
