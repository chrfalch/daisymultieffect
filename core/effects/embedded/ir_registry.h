#pragma once
/**
 * Embedded Cabinet IR Registry (Shared)
 *
 * This file provides a registry of all embedded cabinet impulse responses
 * for selection via enum parameter in the Cabinet IR effect.
 *
 * Shared between firmware and VST builds.
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
#include "irs/Mesa_Oversized_V30_SM57_1___jp_is_out_of_tune.h"
#include "irs/Mesa_Oversized_V30_SM57_2___jp_is_out_of_tune.h"
#include "irs/Mesa_Oversized_V30_SM58_1___jp_is_out_of_tune.h"
#include "irs/Mesa_Oversized_V30_SM58_2___jp_is_out_of_tune.h"
#include "irs/Mesa_Oversized_V30_AT2020_1___jp_is_out_of_tune.h"
#include "irs/Mesa_Oversized_V30_AT2020_2___jp_is_out_of_tune.h"
#include "irs/IR_SM57_V30_4_Raw.h"
#include "irs/IR_SM58_V30_3_Raw.h"

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
        Mesa_V30_SM57_1 = 4,
        Mesa_V30_SM57_2 = 5,
        Mesa_V30_SM58_1 = 6,
        Mesa_V30_SM58_2 = 7,
        Mesa_V30_AT2020_1 = 8,
        Mesa_V30_AT2020_2 = 9,
        Mesa_V30_SM57_Raw = 10,
        Mesa_V30_SM58_Raw = 11,
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
        // 4: Mesa Oversized V30 SM57 1
        {
            Mesa_Oversized_V30_SM57_1___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_SM57_1___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_SM57_1___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_SM57_1___jp_is_out_of_tune::kSamples,
        },
        // 5: Mesa Oversized V30 SM57 2
        {
            Mesa_Oversized_V30_SM57_2___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_SM57_2___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_SM57_2___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_SM57_2___jp_is_out_of_tune::kSamples,
        },
        // 6: Mesa Oversized V30 SM58 1
        {
            Mesa_Oversized_V30_SM58_1___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_SM58_1___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_SM58_1___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_SM58_1___jp_is_out_of_tune::kSamples,
        },
        // 7: Mesa Oversized V30 SM58 2
        {
            Mesa_Oversized_V30_SM58_2___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_SM58_2___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_SM58_2___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_SM58_2___jp_is_out_of_tune::kSamples,
        },
        // 8: Mesa Oversized V30 AT2020 1
        {
            Mesa_Oversized_V30_AT2020_1___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_AT2020_1___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_AT2020_1___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_AT2020_1___jp_is_out_of_tune::kSamples,
        },
        // 9: Mesa Oversized V30 AT2020 2
        {
            Mesa_Oversized_V30_AT2020_2___jp_is_out_of_tune::kName,
            Mesa_Oversized_V30_AT2020_2___jp_is_out_of_tune::kSampleRate,
            Mesa_Oversized_V30_AT2020_2___jp_is_out_of_tune::kLength,
            Mesa_Oversized_V30_AT2020_2___jp_is_out_of_tune::kSamples,
        },
        // 10: Mesa V30 SM57 Raw
        {
            IR_SM57_V30_4_Raw::kName,
            IR_SM57_V30_4_Raw::kSampleRate,
            IR_SM57_V30_4_Raw::kLength,
            IR_SM57_V30_4_Raw::kSamples,
        },
        // 11: Mesa V30 SM58 Raw
        {
            IR_SM58_V30_3_Raw::kName,
            IR_SM58_V30_3_Raw::kSampleRate,
            IR_SM58_V30_3_Raw::kLength,
            IR_SM58_V30_3_Raw::kSamples,
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
