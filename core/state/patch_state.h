#pragma once

#include "patch_observer.h"
#include "../patch/patch_protocol.h"
#include <vector>
#include <mutex>
#include <algorithm>

namespace daisyfx
{

    /**
     * PatchState - Single Source of Truth for all patch data.
     *
     * This class owns the canonical patch state. All changes go through here,
     * and all observers are notified after changes are applied.
     *
     * Key principles:
     * - Changes are deduplicated (no notification if value unchanged)
     * - Thread-safe for observer registration
     * - Observers notified synchronously after state change
     */
    class PatchState
    {
    public:
        static constexpr uint8_t MAX_SLOTS = 12;
        static constexpr uint8_t MAX_PARAMS = 8;
        static constexpr uint8_t NUM_BUTTONS = 2;

        PatchState()
        {
            initializeDefault();
        }

        //=========================================================================
        // Observer Management
        //=========================================================================

        void addObserver(PatchObserver *observer)
        {
            std::lock_guard<std::mutex> lock(observerMutex_);
            if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end())
            {
                observers_.push_back(observer);
            }
        }

        void removeObserver(PatchObserver *observer)
        {
            std::lock_guard<std::mutex> lock(observerMutex_);
            observers_.erase(
                std::remove(observers_.begin(), observers_.end(), observer),
                observers_.end());
        }

        //=========================================================================
        // State Mutators (Commands) - Apply change and notify
        //=========================================================================

        void setSlotEnabled(uint8_t slot, bool enabled)
        {
            if (slot >= MAX_SLOTS)
                return;

            uint8_t newVal = enabled ? 1 : 0;
            if (patch_.slots[slot].enabled == newVal)
                return; // No change

            patch_.slots[slot].enabled = newVal;
            notifySlotEnabledChanged(slot, enabled);
        }

        void setSlotType(uint8_t slot, uint8_t typeId)
        {
            if (slot >= MAX_SLOTS)
                return;
            if (patch_.slots[slot].typeId == typeId)
                return; // No change

            patch_.slots[slot].typeId = typeId;
            initializeDefaultParams(slot, typeId);
            notifySlotTypeChanged(slot, typeId);
        }

        void setSlotParam(uint8_t slot, uint8_t paramId, uint8_t value)
        {
            if (slot >= MAX_SLOTS || paramId >= MAX_PARAMS)
                return;

            auto &slotData = patch_.slots[slot];

            // Find existing param or add new
            for (int i = 0; i < slotData.numParams; ++i)
            {
                if (slotData.params[i].id == paramId)
                {
                    if (slotData.params[i].value == value)
                        return; // No change
                    slotData.params[i].value = value;
                    notifySlotParamChanged(slot, paramId, value);
                    return;
                }
            }

            // Add new param if space available
            if (slotData.numParams < MAX_PARAMS)
            {
                slotData.params[slotData.numParams].id = paramId;
                slotData.params[slotData.numParams].value = value;
                slotData.numParams++;
                notifySlotParamChanged(slot, paramId, value);
            }
        }

        void setSlotMix(uint8_t slot, uint8_t wet, uint8_t dry)
        {
            if (slot >= MAX_SLOTS)
                return;
            if (patch_.slots[slot].wet == wet && patch_.slots[slot].dry == dry)
                return;

            patch_.slots[slot].wet = wet;
            patch_.slots[slot].dry = dry;
            notifySlotMixChanged(slot, wet, dry);
        }

        void setSlotRouting(uint8_t slot, uint8_t inputL, uint8_t inputR)
        {
            if (slot >= MAX_SLOTS)
                return;
            if (patch_.slots[slot].inputL == inputL && patch_.slots[slot].inputR == inputR)
                return;

            patch_.slots[slot].inputL = inputL;
            patch_.slots[slot].inputR = inputR;
            notifySlotRoutingChanged(slot, inputL, inputR);
        }

        void setSlotSumToMono(uint8_t slot, bool sumToMono)
        {
            if (slot >= MAX_SLOTS)
                return;

            uint8_t newVal = sumToMono ? 1 : 0;
            if (patch_.slots[slot].sumToMono == newVal)
                return;

            patch_.slots[slot].sumToMono = newVal;
            notifySlotSumToMonoChanged(slot, sumToMono);
        }

        void setSlotChannelPolicy(uint8_t slot, uint8_t channelPolicy)
        {
            if (slot >= MAX_SLOTS)
                return;
            if (patch_.slots[slot].channelPolicy == channelPolicy)
                return;

            patch_.slots[slot].channelPolicy = channelPolicy;
            notifySlotChannelPolicyChanged(slot, channelPolicy);
        }

        void setTempo(float bpm)
        {
            if (tempo_ == bpm)
                return;
            tempo_ = bpm;
            notifyTempoChanged(bpm);
        }

        /**
         * Set input gain in dB. Range: 0 to +24 dB.
         * Clamped for safety.
         */
        void setInputGainDb(float gainDb)
        {
            // Clamp to safe range: 0dB to +24dB
            if (gainDb < 0.0f)
                gainDb = 0.0f;
            if (gainDb > 24.0f)
                gainDb = 24.0f;

            if (inputGainDb_ == gainDb)
                return;
            inputGainDb_ = gainDb;
            notifyInputGainChanged(gainDb);
        }

        /**
         * Set output gain in dB. Range: -12 to +12 dB.
         * Clamped for safety.
         */
        void setOutputGainDb(float gainDb)
        {
            // Clamp to safe range: -12dB to +12dB
            if (gainDb < -12.0f)
                gainDb = -12.0f;
            if (gainDb > 12.0f)
                gainDb = 12.0f;

            if (outputGainDb_ == gainDb)
                return;
            outputGainDb_ = gainDb;
            notifyOutputGainChanged(gainDb);
        }

        /**
         * Load a complete patch. Replaces current state and notifies onPatchLoaded.
         */
        void loadPatch(const PatchWireDesc &patch)
        {
            patch_ = patch;
            notifyPatchLoaded();
        }

        //=========================================================================
        // State Accessors (Read-Only)
        //=========================================================================

        const PatchWireDesc &getPatch() const { return patch_; }

        bool isSlotEnabled(uint8_t slot) const
        {
            return slot < MAX_SLOTS && patch_.slots[slot].enabled != 0;
        }

        uint8_t getSlotType(uint8_t slot) const
        {
            return slot < MAX_SLOTS ? patch_.slots[slot].typeId : 0;
        }

        uint8_t getSlotParam(uint8_t slot, uint8_t paramId) const
        {
            if (slot >= MAX_SLOTS)
                return 0;
            const auto &slotData = patch_.slots[slot];
            for (int i = 0; i < slotData.numParams; ++i)
            {
                if (slotData.params[i].id == paramId)
                {
                    return slotData.params[i].value;
                }
            }
            return 0;
        }

        uint8_t getNumSlots() const { return patch_.numSlots; }
        float getTempo() const { return tempo_; }
        float getInputGainDb() const { return inputGainDb_; }
        float getOutputGainDb() const { return outputGainDb_; }

    private:
        void initializeDefault()
        {
            patch_ = {};
            patch_.numSlots = 4;
            tempo_ = 120.0f;

            // Initialize slot indices
            for (uint8_t i = 0; i < MAX_SLOTS; ++i)
            {
                patch_.slots[i].slotIndex = i;
                patch_.slots[i].enabled = 1;
                patch_.slots[i].wet = 127;
                patch_.slots[i].dry = 0;
            }
        }

        void initializeDefaultParams(uint8_t slot, uint8_t typeId)
        {
            auto &s = patch_.slots[slot];
            s.numParams = 0;
            s.sumToMono = 0;

            switch (typeId)
            {
            case 1: // Delay
                s.numParams = 5;
                s.params[0] = {0, 64};  // Time
                s.params[1] = {1, 32};  // Division
                s.params[2] = {2, 127}; // Sync
                s.params[3] = {3, 50};  // Feedback
                s.params[4] = {4, 80};  // Mix
                break;
            case 10: // Distortion
                s.numParams = 2;
                s.sumToMono = 1;
                s.params[0] = {0, 40}; // Drive
                s.params[1] = {1, 64}; // Tone
                break;
            case 12: // Sweep Delay
                s.numParams = 7;
                s.params[0] = {0, 64};
                s.params[1] = {1, 32};
                s.params[2] = {2, 127};
                s.params[3] = {3, 50};
                s.params[4] = {4, 80};
                s.params[5] = {5, 64};
                s.params[6] = {6, 32};
                break;
            case 13: // Mixer
                s.numParams = 3;
                s.params[0] = {0, 64};
                s.params[1] = {1, 64};
                s.params[2] = {2, 0};
                break;
            case 14: // Reverb
                s.numParams = 5;
                s.params[0] = {0, 50};
                s.params[1] = {1, 70};
                s.params[2] = {2, 40};
                s.params[3] = {3, 30};
                s.params[4] = {4, 64};
                break;
            case 15: // Compressor
                s.numParams = 5;
                s.params[0] = {0, 64};
                s.params[1] = {1, 32};
                s.params[2] = {2, 32};
                s.params[3] = {3, 64};
                s.params[4] = {4, 32};
                break;
            case 16: // Chorus
                s.numParams = 5;
                s.params[0] = {0, 40};
                s.params[1] = {1, 64};
                s.params[2] = {2, 0};
                s.params[3] = {3, 64};
                s.params[4] = {4, 64};
                break;
            case 17: // Noise Gate
                s.numParams = 5;
                s.params[0] = {0, 64};  // Threshold
                s.params[1] = {1, 32};  // Attack
                s.params[2] = {2, 64};  // Hold
                s.params[3] = {3, 64};  // Release
                s.params[4] = {4, 0};   // Range
                break;
            case 18: // Graphic EQ
                s.numParams = 7;
                s.params[0] = {0, 64};  // 100Hz (0dB = center)
                s.params[1] = {1, 64};  // 200Hz
                s.params[2] = {2, 64};  // 400Hz
                s.params[3] = {3, 64};  // 800Hz
                s.params[4] = {4, 64};  // 1.6kHz
                s.params[5] = {5, 64};  // 3.2kHz
                s.params[6] = {6, 64};  // 6.4kHz
                break;
            case 19: // Flanger
                s.numParams = 5;
                s.params[0] = {0, 40};  // Rate
                s.params[1] = {1, 64};  // Depth
                s.params[2] = {2, 64};  // Feedback (center = 0)
                s.params[3] = {3, 32};  // Delay
                s.params[4] = {4, 64};  // Mix
                break;
            case 20: // Phaser
                s.numParams = 5;
                s.params[0] = {0, 40};  // Rate
                s.params[1] = {1, 64};  // Depth
                s.params[2] = {2, 32};  // Feedback
                s.params[3] = {3, 64};  // Freq
                s.params[4] = {4, 64};  // Mix
                break;
            case 21: // Neural Amp
                s.numParams = 6;
                s.sumToMono = 1;
                s.params[0] = {0, 0};   // Model (enum: Fender 57)
                s.params[1] = {1, 64};  // Input
                s.params[2] = {2, 64};  // Output
                s.params[3] = {3, 64};  // Bass
                s.params[4] = {4, 64};  // Mid
                s.params[5] = {5, 64};  // Treble
                break;
            case 22: // Cabinet IR
                s.numParams = 5;
                s.params[0] = {0, 0};   // Cabinet (enum: first IR)
                s.params[1] = {1, 127}; // Mix (100%)
                s.params[2] = {2, 64};  // Output
                s.params[3] = {3, 0};   // Low Cut (off)
                s.params[4] = {4, 0};   // High Cut (off)
                break;
            default:
                s.numParams = 0;
                break;
            }
        }

        //=========================================================================
        // Notification Helpers
        // NOTE: Copy observers_ under lock, then notify without lock to avoid deadlock
        //=========================================================================

        void notifySlotEnabledChanged(uint8_t slot, bool enabled)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotEnabledChanged(slot, enabled);
        }

        void notifySlotTypeChanged(uint8_t slot, uint8_t typeId)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotTypeChanged(slot, typeId);
        }

        void notifySlotParamChanged(uint8_t slot, uint8_t paramId, uint8_t value)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotParamChanged(slot, paramId, value);
        }

        void notifySlotMixChanged(uint8_t slot, uint8_t wet, uint8_t dry)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotMixChanged(slot, wet, dry);
        }

        void notifySlotRoutingChanged(uint8_t slot, uint8_t inputL, uint8_t inputR)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotRoutingChanged(slot, inputL, inputR);
        }

        void notifySlotSumToMonoChanged(uint8_t slot, bool sumToMono)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotSumToMonoChanged(slot, sumToMono);
        }

        void notifySlotChannelPolicyChanged(uint8_t slot, uint8_t channelPolicy)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onSlotChannelPolicyChanged(slot, channelPolicy);
        }

        void notifyPatchLoaded()
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onPatchLoaded();
        }

        void notifyTempoChanged(float bpm)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onTempoChanged(bpm);
        }

        void notifyInputGainChanged(float gainDb)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onInputGainChanged(gainDb);
        }

        void notifyOutputGainChanged(float gainDb)
        {
            std::vector<PatchObserver *> obs;
            {
                std::lock_guard<std::mutex> lock(observerMutex_);
                obs = observers_;
            }
            for (auto *o : obs)
                o->onOutputGainChanged(gainDb);
        }

        PatchWireDesc patch_;
        float tempo_ = 120.0f;
        float inputGainDb_ = 18.0f; // Default +18dB for instrument level
        float outputGainDb_ = 0.0f; // Default unity

        std::vector<PatchObserver *> observers_;
        std::mutex observerMutex_;
    };

} // namespace daisyfx
