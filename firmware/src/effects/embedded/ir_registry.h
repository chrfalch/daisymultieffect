#pragma once
/**
 * Embedded Cabinet IR Registry
 *
 * This file provides a registry of all embedded cabinet impulse responses
 * for selection via enum parameter in the Cabinet IR effect.
 *
 * Auto-generated IR headers are included from embedded/irs/
 *
 * NOTE: V1 is mono only. Stereo IR support deferred to V2.
 */

#include <cstdint>
#include <cstddef>

// Include all embedded IR headers
#include "irs/V30_P1_opus87_deerinkstudios.h"
#include "irs/V30_P1_sene935_deerinkstudios.h"
#include "irs/V30_P2_audix_i5_deerinkstudios.h"
#include "irs/V30_P2_sene935_deerinkstudios.h"

namespace EmbeddedIRs
{

    /**
     * IR info structure for runtime lookup
     */
    struct IRInfo
    {
        const char *name;     // Display name
        int sampleRate;       // Sample rate (should be 48000 for all)
        int length;           // Number of samples
        const float *samples; // IR sample data
    };

    /**
     * Enum for IR selection (matches parameter enum options)
     */
    enum class IR : uint8_t
    {
        V30_P1_Opus87 = 0,
        V30_P1_Sene935 = 1,
        V30_P2_Audix_i5 = 2,
        V30_P2_Sene935 = 3,
        COUNT
    };

    constexpr size_t kNumIRs = static_cast<size_t>(IR::COUNT);

    /**
     * Registry of all embedded IRs
     * Index matches IR enum value
     */
    inline const IRInfo kIRRegistry[] = {
        // 0: V30 Position 1 - Opus 87
        {
            V30_P1_opus87_deerinkstudios::kName,
            V30_P1_opus87_deerinkstudios::kSampleRate,
            V30_P1_opus87_deerinkstudios::kLength,
            V30_P1_opus87_deerinkstudios::kSamples,
        },
        // 1: V30 Position 1 - Sennheiser e935
        {
            V30_P1_sene935_deerinkstudios::kName,
            V30_P1_sene935_deerinkstudios::kSampleRate,
            V30_P1_sene935_deerinkstudios::kLength,
            V30_P1_sene935_deerinkstudios::kSamples,
        },
        // 2: V30 Position 2 - Audix i5
        {
            V30_P2_audix_i5_deerinkstudios::kName,
            V30_P2_audix_i5_deerinkstudios::kSampleRate,
            V30_P2_audix_i5_deerinkstudios::kLength,
            V30_P2_audix_i5_deerinkstudios::kSamples,
        },
        // 3: V30 Position 2 - Sennheiser e935
        {
            V30_P2_sene935_deerinkstudios::kName,
            V30_P2_sene935_deerinkstudios::kSampleRate,
            V30_P2_sene935_deerinkstudios::kLength,
            V30_P2_sene935_deerinkstudios::kSamples,
        },
    };

    /**
     * Get IR info by index
     * Returns nullptr if index out of range
     */
    inline const IRInfo *GetIR(size_t index)
    {
        if (index < kNumIRs)
        {
            return &kIRRegistry[index];
        }
        return nullptr;
    }

    /**
     * Get IR info by enum
     */
    inline const IRInfo *GetIR(IR ir)
    {
        return GetIR(static_cast<size_t>(ir));
    }

} // namespace EmbeddedIRs
