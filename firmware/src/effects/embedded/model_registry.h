#pragma once
/**
 * Embedded Neural Amp Model Registry
 *
 * This file provides a registry of all embedded LSTM-12 amp models
 * for selection via enum parameter in the Neural Amp effect.
 *
 * Auto-generated model headers are included from embedded/models/
 */

#include <cstdint>
#include <cstddef>

// Include all embedded model headers
#include "models/tw40_california_clean_deerinkstudios.h"
#include "models/tw40_california_crunch_deerinkstudios.h"
#include "models/tw40_blues_solo_deerinkstudios.h"
#include "models/tw40_british_rhythm_deerinkstudios.h"
#include "models/tw40_british_lead_deerinkstudios.h"

namespace EmbeddedModels
{

    /**
     * Model info structure for runtime lookup
     */
    struct ModelInfo
    {
        const char *name;    // Display name
        int hiddenSize;      // Hidden layer size (should be 12 for all)
        const float *kernel; // Input kernel weights
        size_t kernelSize;
        const float *recurrent; // Recurrent kernel weights
        size_t recurrentSize;
        const float *bias; // Biases
        size_t biasSize;
        const float *denseW; // Dense output weights
        size_t denseWSize;
        const float *denseB; // Dense output bias
        size_t denseBSize;
    };

    /**
     * Enum for model selection (matches parameter enum options)
     */
    enum class Model : uint8_t
    {
        TW40_Clean = 0,
        TW40_Crunch = 1,
        TW40_Blues = 2,
        TW40_Rhythm = 3,
        TW40_Lead = 4,
        COUNT
    };

    constexpr size_t kNumModels = static_cast<size_t>(Model::COUNT);

    /**
     * Registry of all embedded models
     * Index matches Model enum value
     */
    inline const ModelInfo kModelRegistry[] = {
        // 0: TW40 California Clean
        {
            tw40_california_clean_deerinkstudios::kName,
            tw40_california_clean_deerinkstudios::kHiddenSize,
            tw40_california_clean_deerinkstudios::kKernel,
            tw40_california_clean_deerinkstudios::kKernelSize,
            tw40_california_clean_deerinkstudios::kRecurrent,
            tw40_california_clean_deerinkstudios::kRecurrentSize,
            tw40_california_clean_deerinkstudios::kBias,
            tw40_california_clean_deerinkstudios::kBiasSize,
            tw40_california_clean_deerinkstudios::kDenseW,
            tw40_california_clean_deerinkstudios::kDenseWSize,
            tw40_california_clean_deerinkstudios::kDenseB,
            tw40_california_clean_deerinkstudios::kDenseBSize,
        },
        // 1: TW40 California Crunch
        {
            tw40_california_crunch_deerinkstudios::kName,
            tw40_california_crunch_deerinkstudios::kHiddenSize,
            tw40_california_crunch_deerinkstudios::kKernel,
            tw40_california_crunch_deerinkstudios::kKernelSize,
            tw40_california_crunch_deerinkstudios::kRecurrent,
            tw40_california_crunch_deerinkstudios::kRecurrentSize,
            tw40_california_crunch_deerinkstudios::kBias,
            tw40_california_crunch_deerinkstudios::kBiasSize,
            tw40_california_crunch_deerinkstudios::kDenseW,
            tw40_california_crunch_deerinkstudios::kDenseWSize,
            tw40_california_crunch_deerinkstudios::kDenseB,
            tw40_california_crunch_deerinkstudios::kDenseBSize,
        },
        // 2: TW40 Blues Solo
        {
            tw40_blues_solo_deerinkstudios::kName,
            tw40_blues_solo_deerinkstudios::kHiddenSize,
            tw40_blues_solo_deerinkstudios::kKernel,
            tw40_blues_solo_deerinkstudios::kKernelSize,
            tw40_blues_solo_deerinkstudios::kRecurrent,
            tw40_blues_solo_deerinkstudios::kRecurrentSize,
            tw40_blues_solo_deerinkstudios::kBias,
            tw40_blues_solo_deerinkstudios::kBiasSize,
            tw40_blues_solo_deerinkstudios::kDenseW,
            tw40_blues_solo_deerinkstudios::kDenseWSize,
            tw40_blues_solo_deerinkstudios::kDenseB,
            tw40_blues_solo_deerinkstudios::kDenseBSize,
        },
        // 3: TW40 British Rhythm
        {
            tw40_british_rhythm_deerinkstudios::kName,
            tw40_british_rhythm_deerinkstudios::kHiddenSize,
            tw40_british_rhythm_deerinkstudios::kKernel,
            tw40_british_rhythm_deerinkstudios::kKernelSize,
            tw40_british_rhythm_deerinkstudios::kRecurrent,
            tw40_british_rhythm_deerinkstudios::kRecurrentSize,
            tw40_british_rhythm_deerinkstudios::kBias,
            tw40_british_rhythm_deerinkstudios::kBiasSize,
            tw40_british_rhythm_deerinkstudios::kDenseW,
            tw40_british_rhythm_deerinkstudios::kDenseWSize,
            tw40_british_rhythm_deerinkstudios::kDenseB,
            tw40_british_rhythm_deerinkstudios::kDenseBSize,
        },
        // 4: TW40 British Lead
        {
            tw40_british_lead_deerinkstudios::kName,
            tw40_british_lead_deerinkstudios::kHiddenSize,
            tw40_british_lead_deerinkstudios::kKernel,
            tw40_british_lead_deerinkstudios::kKernelSize,
            tw40_british_lead_deerinkstudios::kRecurrent,
            tw40_british_lead_deerinkstudios::kRecurrentSize,
            tw40_british_lead_deerinkstudios::kBias,
            tw40_british_lead_deerinkstudios::kBiasSize,
            tw40_british_lead_deerinkstudios::kDenseW,
            tw40_british_lead_deerinkstudios::kDenseWSize,
            tw40_british_lead_deerinkstudios::kDenseB,
            tw40_british_lead_deerinkstudios::kDenseBSize,
        },
    };

    /**
     * Get model info by index
     * Returns nullptr if index out of range
     */
    inline const ModelInfo *GetModel(size_t index)
    {
        if (index < kNumModels)
        {
            return &kModelRegistry[index];
        }
        return nullptr;
    }

    /**
     * Get model info by enum
     */
    inline const ModelInfo *GetModel(Model model)
    {
        return GetModel(static_cast<size_t>(model));
    }

} // namespace EmbeddedModels
