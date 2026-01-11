#pragma once

/**
 * @file default_patch.h
 * @brief Shared default patch configuration
 *
 * This file defines the default "guitar signal chain" patch used
 * by both VST and firmware. It provides a starting point with:
 * - Gate -> Compressor -> Distortion -> EQ -> Chorus -> Reverb
 *
 * The remaining slots are empty and ready for configuration.
 *
 * Uses only protocol definitions - no dependency on effect_metadata.h
 */

#include "protocol/sysex_protocol.h"
#include <cstdint>

namespace daisyfx
{
    // Import types from protocol namespace
    using DaisyMultiFX::Protocol::ButtonMode;
    using DaisyMultiFX::Protocol::ChannelPolicy;
    using DaisyMultiFX::Protocol::PatchWireDesc;
    using DaisyMultiFX::Protocol::ROUTE_INPUT;

    // Number of effect slots in the system
    static constexpr int kNumSlots = 12;

    // Maximum params per effect slot (should match largest effect)
    static constexpr int kMaxParamsPerSlot = 7;

    /**
     * Default slot configuration for parameter defaults.
     * Used by VST for APVTS parameter layout defaults.
     */
    struct DefaultSlotConfig
    {
        uint8_t typeId;                  // Effect type ID (from SYSEX_EFFECT_* constants)
        float params[kMaxParamsPerSlot]; // Normalized parameters (0-1)
    };

    /**
     * Guitar signal chain default configuration:
     * Slot 0: Noise Gate
     * Slot 1: Compressor
     * Slot 2: Distortion
     * Slot 3: GraphicEQ (7 bands)
     * Slot 4: Chorus
     * Slot 5: Reverb
     * Slots 6-11: Empty
     */
    inline const DefaultSlotConfig kDefaultSlots[kNumSlots] = {
        // Slot 0: Noise Gate
        {SYSEX_EFFECT_NOISE_GATE, {64.0f / 127.0f, 20.0f / 127.0f, 50.0f / 127.0f, 40.0f / 127.0f, 0.0f / 127.0f, 0.5f, 0.5f}},
        // Slot 1: Compressor
        {SYSEX_EFFECT_COMPRESSOR, {80.0f / 127.0f, 16.0f / 127.0f, 40.0f / 127.0f, 50.0f / 127.0f, 20.0f / 127.0f, 0.5f, 0.5f}},
        // Slot 2: Distortion
        {SYSEX_EFFECT_DISTORTION, {40.0f / 127.0f, 70.0f / 127.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        // Slot 3: GraphicEQ - 7 bands
        {SYSEX_EFFECT_GRAPHIC_EQ, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        // Slot 4: Chorus
        {SYSEX_EFFECT_CHORUS, {30.0f / 127.0f, 50.0f / 127.0f, 20.0f / 127.0f, 40.0f / 127.0f, 50.0f / 127.0f, 0.5f, 0.5f}},
        // Slot 5: Reverb
        {SYSEX_EFFECT_REVERB, {40.0f / 127.0f, 50.0f / 127.0f, 60.0f / 127.0f, 25.0f / 127.0f, 50.0f / 127.0f, 0.5f, 0.5f}},
        // Slots 6-11: Empty (Off)
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {SYSEX_EFFECT_OFF, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
    };

    /**
     * Creates the default guitar signal chain patch.
     * This is the initial patch loaded when no saved patch exists.
     *
     * @return A PatchWireDesc with the default signal chain configured
     */
    inline PatchWireDesc MakeDefaultPatch()
    {
        PatchWireDesc patch = {};
        patch.numSlots = kNumSlots;

        for (int slot = 0; slot < kNumSlots; ++slot)
        {
            const auto &def = kDefaultSlots[slot];
            patch.slots[slot].slotIndex = static_cast<uint8_t>(slot);
            patch.slots[slot].typeId = def.typeId;
            patch.slots[slot].enabled = (def.typeId != SYSEX_EFFECT_OFF) ? 1 : 0;
            patch.slots[slot].inputL = (slot == 0) ? ROUTE_INPUT : static_cast<uint8_t>(slot - 1);
            patch.slots[slot].inputR = (slot == 0) ? ROUTE_INPUT : static_cast<uint8_t>(slot - 1);
            patch.slots[slot].sumToMono = (slot == 0) ? 1 : 0;
            patch.slots[slot].wet = 127;
            patch.slots[slot].dry = 0;
            patch.slots[slot].channelPolicy = static_cast<uint8_t>(ChannelPolicy::Auto);
            patch.slots[slot].numParams = kMaxParamsPerSlot;
            for (int p = 0; p < kMaxParamsPerSlot; ++p)
            {
                patch.slots[slot].params[p] = {static_cast<uint8_t>(p),
                                               static_cast<uint8_t>(def.params[p] * 127.0f)};
            }
        }

        // Button assignments
        patch.buttons[0] = {0, ButtonMode::ToggleBypass};
        patch.buttons[1] = {0, ButtonMode::TapTempo};

        return patch;
    }

} // namespace daisyfx
