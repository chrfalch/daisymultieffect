#pragma once

#include <cstdint>

namespace daisyfx
{

    /**
     * Observer interface for PatchState changes.
     * Implement this to be notified when patch state changes.
     * All methods have default empty implementations so you only override what you need.
     */
    class PatchObserver
    {
    public:
        virtual ~PatchObserver() = default;

        // Slot state changes
        virtual void onSlotEnabledChanged(uint8_t slot, bool enabled) {}
        virtual void onSlotTypeChanged(uint8_t slot, uint8_t typeId) {}
        virtual void onSlotParamChanged(uint8_t slot, uint8_t paramId, uint8_t value) {}
        virtual void onSlotMixChanged(uint8_t slot, uint8_t wet, uint8_t dry) {}
        virtual void onSlotRoutingChanged(uint8_t slot, uint8_t inputL, uint8_t inputR) {}

        // Full patch changes
        virtual void onPatchLoaded() {}

        // Global changes
        virtual void onTempoChanged(float bpm) {}
    };

} // namespace daisyfx
