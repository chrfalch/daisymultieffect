/**
 * @file sysex_protocol_c.h
 * @brief C-compatible SysEx protocol definitions for DaisyMultiFX
 *
 * This header provides C-compatible definitions that can be used by:
 * - C code
 * - C++ code (via sysex_protocol.h wrapper)
 * - Swift code (via bridging header)
 *
 * All messages use manufacturer ID 0x7D (educational/development use).
 *
 * NOTE: We use anonymous enums so Swift can import them.
 * Swift cannot import #define macros or static const variables.
 */

#ifndef SYSEX_PROTOCOL_C_H
#define SYSEX_PROTOCOL_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Constants - Using anonymous enums so Swift can import them
     * ============================================================================= */

    enum
    {
        /** MIDI SysEx manufacturer ID (0x7D = educational/development use) */
        SYSEX_MANUFACTURER_ID = 0x7D,

        /** Maximum number of effect slots */
        SYSEX_MAX_SLOTS = 12,

        /** Maximum parameters per effect slot */
        SYSEX_MAX_PARAMS_PER_SLOT = 8,

        /** Number of hardware buttons */
        SYSEX_NUM_BUTTONS = 2,

        /** Special route value indicating hardware input */
        SYSEX_ROUTE_INPUT = 255
    };

    /* =============================================================================
     * Message Types - Host to Device (Commands)
     * ============================================================================= */

    enum
    {
        /** Request patch dump: F0 7D 12 F7 */
        SYSEX_CMD_REQUEST_PATCH = 0x12,

        /** Set parameter: F0 7D 20 <slot> <paramId> <value> F7 */
        SYSEX_CMD_SET_PARAM = 0x20,

        /** Set slot enabled: F0 7D 21 <slot> <enabled> F7 */
        SYSEX_CMD_SET_ENABLED = 0x21,

        /** Set slot effect type: F0 7D 22 <slot> <typeId> F7 */
        SYSEX_CMD_SET_TYPE = 0x22,

        /** Set slot routing: F0 7D <sender> 23 <slot> <inputL> <inputR> F7 */
        SYSEX_CMD_SET_ROUTING = 0x23,

        /** Set slot sum-to-mono flag: F0 7D <sender> 24 <slot> <sumToMono> F7 */
        SYSEX_CMD_SET_SUM_TO_MONO = 0x24,

        /** Set slot mix: F0 7D <sender> 25 <slot> <dry> <wet> F7 */
        SYSEX_CMD_SET_MIX = 0x25,

        /** Set slot channel policy: F0 7D <sender> 26 <slot> <policy> F7 */
        SYSEX_CMD_SET_CHANNEL_POLICY = 0x26,

        /** Request all effect metadata: F0 7D 32 F7 */
        SYSEX_CMD_REQUEST_EFFECT_META = 0x32
    };

    /* =============================================================================
     * Message Types - Device to Host (Responses)
     * ============================================================================= */

    enum
    {
        /**
         * Patch dump response
         * F0 7D 13 <numSlots>
         *   12x slots: slotIndex typeId enabled inputL inputR sumToMono dry wet policy numParams (id val)x8
         *   2x buttons: slotIndex mode
         * F7
         */
        SYSEX_RESP_PATCH_DUMP = 0x13,

        /** Effect meta list (all effects): F0 7D 33 <numEffects> [effect data...] F7 */
        SYSEX_RESP_EFFECT_META_LIST = 0x33,

        /** Effect discovered (single effect): F0 7D 34 <typeId> <nameLen> <name...> F7 */
        SYSEX_RESP_EFFECT_DISCOVERED = 0x34,

        /**
         * Effect metadata v2 (with params):
         * F0 7D 35 <typeId> <nameLen> <name...> <numParams> (paramId kind nameLen name...)xN F7
         */
        SYSEX_RESP_EFFECT_META_V2 = 0x35,

        /**
         * Effect metadata v3 (with params and short name):
         * F0 7D 36 <typeId> <nameLen> <name...> <shortName[3]> <numParams> (paramId kind nameLen name...)xN F7
         * shortName is always 3 bytes (7-bit ASCII)
         */
        SYSEX_RESP_EFFECT_META_V3 = 0x36,

        /**
         * Effect metadata v4 (with params, short name, and number ranges):
         * F0 7D <sender> 37 <typeId> <nameLen> <name...> <shortName[3]> <numParams>
         *   (paramId kind flags nameLen name... [minQ16.16_5] [maxQ16.16_5] [stepQ16.16_5]) x N
         * flags bit0: hasNumberRange (only valid when kind==Number)
         */
        SYSEX_RESP_EFFECT_META_V4 = 0x37,

        /**
         * Effect metadata v5 (adds effect/param descriptions + unit prefix/suffix):
         * F0 7D <sender> 38 <typeId> <nameLen> <name...> <shortName[3]> <effectDescLen> <effectDesc...> <numParams>
         *   (paramId kind flags nameLen name... descLen desc... unitPreLen unitPre... unitSufLen unitSuf...
         *    [minQ16.16_5] [maxQ16.16_5] [stepQ16.16_5]) x N
         * flags bit0: hasNumberRange (only valid when kind==Number)
         */
        SYSEX_RESP_EFFECT_META_V5 = 0x38,

        /** Button state change: F0 7D 40 <btn> <slot> <enabled> F7 */
        SYSEX_RESP_BUTTON_STATE = 0x40,

        /** Tempo update: F0 7D 41 <bpmQ16_16_5bytes> F7 */
        SYSEX_RESP_TEMPO_UPDATE = 0x41
    };

    /* =============================================================================
     * Effect Type IDs
     * ============================================================================= */

    enum
    {
        SYSEX_EFFECT_OFF = 0,
        SYSEX_EFFECT_DELAY = 1,
        SYSEX_EFFECT_DISTORTION = 10,
        SYSEX_EFFECT_SWEEP_DELAY = 12,
        SYSEX_EFFECT_MIXER = 13,
        SYSEX_EFFECT_REVERB = 14,
        SYSEX_EFFECT_COMPRESSOR = 15,
        SYSEX_EFFECT_CHORUS = 16
    };

    /* =============================================================================
     * Enums (C-compatible)
     * ============================================================================= */

    /** Channel routing policy for stereo effects */
    typedef enum
    {
        SYSEX_CHANNEL_POLICY_AUTO = 0,        /**< Automatic based on input */
        SYSEX_CHANNEL_POLICY_FORCE_MONO = 1,  /**< Force mono processing */
        SYSEX_CHANNEL_POLICY_FORCE_STEREO = 2 /**< Force stereo processing */
    } SysExChannelPolicy;

    /** Hardware button mode assignment */
    typedef enum
    {
        SYSEX_BUTTON_MODE_UNUSED = 0,        /**< Button not assigned */
        SYSEX_BUTTON_MODE_TOGGLE_BYPASS = 1, /**< Toggle effect bypass */
        SYSEX_BUTTON_MODE_TAP_TEMPO = 2      /**< Tap tempo input */
    } SysExButtonMode;

    /* =============================================================================
     * Wire Structures (C-compatible, for serialization)
     * ============================================================================= */

    /** Button assignment for hardware buttons */
    typedef struct
    {
        uint8_t slotIndex; /**< 0-11 or 127 for unassigned */
        uint8_t mode;      /**< SysExButtonMode value */
    } SysExButtonAssignWire;

    /** Single parameter (id + value pair) */
    typedef struct
    {
        uint8_t id;    /**< Parameter ID within effect */
        uint8_t value; /**< 0..127 */
    } SysExSlotParamWire;

    /** Complete slot description */
    typedef struct
    {
        uint8_t slotIndex; /**< 0-11 */
        uint8_t typeId;    /**< Effect type (see SYSEX_EFFECT_* constants) */
        uint8_t enabled;   /**< 0/1 */

        uint8_t inputL; /**< Left input route (0-11 = slot, SYSEX_ROUTE_INPUT = hw input) */
        uint8_t inputR; /**< Right input route */

        uint8_t sumToMono; /**< Sum inputs to mono (0/1) */
        uint8_t dry;       /**< Dry level 0..127 */
        uint8_t wet;       /**< Wet level 0..127 */

        uint8_t channelPolicy; /**< SysExChannelPolicy enum value */
        uint8_t numParams;     /**< Number of valid params (<= SYSEX_MAX_PARAMS_PER_SLOT) */
        SysExSlotParamWire params[8];
    } SysExSlotWireDesc;

    /** Complete patch description */
    typedef struct
    {
        uint8_t numSlots; /**< Number of active slots (typically 4 or 12) */
        SysExSlotWireDesc slots[12];
        SysExButtonAssignWire buttons[2];
    } SysExPatchWireDesc;

    /* =============================================================================
     * Utility Functions (inline for C99+)
     * ============================================================================= */

    /** Encode a route value for 7-bit safe transmission */
    static inline uint8_t sysex_encode_route(uint8_t route)
    {
        return (route == SYSEX_ROUTE_INPUT) ? 127 : (route & 0x7F);
    }

    /** Decode a route value from 7-bit safe transmission */
    static inline uint8_t sysex_decode_route(uint8_t encoded)
    {
        return (encoded == 127) ? SYSEX_ROUTE_INPUT : encoded;
    }

    /** Convert float to Q16.16 fixed point */
    static inline int32_t sysex_float_to_q16_16(float v)
    {
        return (int32_t)(v * 65536.0f + (v >= 0 ? 0.5f : -0.5f));
    }

    /** Convert Q16.16 fixed point to float */
    static inline float sysex_q16_16_to_float(int32_t v)
    {
        return (float)v / 65536.0f;
    }

    /** Pack Q16.16 value into 5 bytes (7-bit safe) */
    static inline void sysex_pack_q16_16(int32_t value, uint8_t out[5])
    {
        uint32_t u = (uint32_t)value;
        out[0] = u & 0x7F;
        out[1] = (u >> 7) & 0x7F;
        out[2] = (u >> 14) & 0x7F;
        out[3] = (u >> 21) & 0x7F;
        out[4] = (u >> 28) & 0x7F;
    }

    /** Unpack 5 bytes into Q16.16 value */
    static inline int32_t sysex_unpack_q16_16(const uint8_t in[5])
    {
        uint32_t u = 0;
        u |= (uint32_t)in[0];
        u |= (uint32_t)in[1] << 7;
        u |= (uint32_t)in[2] << 14;
        u |= (uint32_t)in[3] << 21;
        u |= (uint32_t)in[4] << 28;
        return (int32_t)u;
    }

#ifdef __cplusplus
}
#endif

#endif /* SYSEX_PROTOCOL_C_H */
