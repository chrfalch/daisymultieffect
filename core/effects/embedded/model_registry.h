#pragma once
/**
 * Embedded Neural Amp Model Registry (Shared)
 *
 * This file provides a registry of all embedded GRU-9 amp models
 * for selection via enum parameter in the Neural Amp effect.
 *
 * Shared between firmware and VST builds.
 *
 * Model weights converted from Mars project (GRU-9 architecture).
 */

#include <cstdint>
#include <cstddef>

// Include GRU-9 model weights
#include "models/gru9_models.h"

namespace EmbeddedModels
{

    /**
     * Model info structure for runtime lookup (GRU-9)
     *
     * GRU has 3 gates (reset, update, new) vs LSTM's 4.
     * Weight layout:
     *   rec_weight_ih: input-to-hidden [1 × 3*hidden] = [1 × 27]
     *   rec_weight_hh: hidden-to-hidden [hidden × 3*hidden] = [9 × 27]
     *   rec_bias: [2 × 3*hidden] = [2 × 27]
     *   lin_weight: dense output [1 × hidden] = [1 × 9]
     *   lin_bias: dense bias [1]
     */
    struct ModelInfo
    {
        const char *name;        // Display name
        int hiddenSize;          // Hidden layer size (9 for all)
        const float *weightIH;   // Input-to-hidden weights
        size_t weightIHSize;
        const float *weightHH;   // Hidden-to-hidden weights
        size_t weightHHSize;
        const float *bias;       // GRU biases (2 rows)
        size_t biasSize;
        const float *denseW;     // Dense output weights
        size_t denseWSize;
        const float *denseB;     // Dense output bias
        size_t denseBSize;
        float levelAdjust;       // Output level adjustment
    };

    /**
     * Enum for model selection (matches parameter enum options)
     */
    enum class Model : uint8_t
    {
        Fender57 = 0,
        Matchless = 1,
        KlonBB = 2,
        MesaIIC = 3,
        HAKClean = 4,
        Bassman = 5,
        EVH5150 = 6,
        Splawn = 7,
        COUNT
    };

    constexpr size_t kNumModels = static_cast<size_t>(Model::COUNT);

    /**
     * Registry of all embedded models
     * Index matches Model enum value
     */
    inline const ModelInfo kModelRegistry[] = {
        // 0: Fender 57 G5
        {
            gru9_fender57::kName,
            gru9_fender57::kHiddenSize,
            gru9_fender57::kWeightIH,
            gru9_fender57::kWeightIHSize,
            gru9_fender57::kWeightHH,
            gru9_fender57::kWeightHHSize,
            gru9_fender57::kBias,
            gru9_fender57::kBiasSize,
            gru9_fender57::kDenseW,
            gru9_fender57::kDenseWSize,
            gru9_fender57::kDenseB,
            gru9_fender57::kDenseBSize,
            gru9_fender57::kLevelAdjust,
        },
        // 1: Matchless SC30
        {
            gru9_matchless::kName,
            gru9_matchless::kHiddenSize,
            gru9_matchless::kWeightIH,
            gru9_matchless::kWeightIHSize,
            gru9_matchless::kWeightHH,
            gru9_matchless::kWeightHHSize,
            gru9_matchless::kBias,
            gru9_matchless::kBiasSize,
            gru9_matchless::kDenseW,
            gru9_matchless::kDenseWSize,
            gru9_matchless::kDenseB,
            gru9_matchless::kDenseBSize,
            gru9_matchless::kLevelAdjust,
        },
        // 2: Klon BB
        {
            gru9_klon_bb::kName,
            gru9_klon_bb::kHiddenSize,
            gru9_klon_bb::kWeightIH,
            gru9_klon_bb::kWeightIHSize,
            gru9_klon_bb::kWeightHH,
            gru9_klon_bb::kWeightHHSize,
            gru9_klon_bb::kBias,
            gru9_klon_bb::kBiasSize,
            gru9_klon_bb::kDenseW,
            gru9_klon_bb::kDenseWSize,
            gru9_klon_bb::kDenseB,
            gru9_klon_bb::kDenseBSize,
            gru9_klon_bb::kLevelAdjust,
        },
        // 3: Mesa IIC
        {
            gru9_mesa_iic::kName,
            gru9_mesa_iic::kHiddenSize,
            gru9_mesa_iic::kWeightIH,
            gru9_mesa_iic::kWeightIHSize,
            gru9_mesa_iic::kWeightHH,
            gru9_mesa_iic::kWeightHHSize,
            gru9_mesa_iic::kBias,
            gru9_mesa_iic::kBiasSize,
            gru9_mesa_iic::kDenseW,
            gru9_mesa_iic::kDenseWSize,
            gru9_mesa_iic::kDenseB,
            gru9_mesa_iic::kDenseBSize,
            gru9_mesa_iic::kLevelAdjust,
        },
        // 4: HAK Clean
        {
            gru9_hak_clean::kName,
            gru9_hak_clean::kHiddenSize,
            gru9_hak_clean::kWeightIH,
            gru9_hak_clean::kWeightIHSize,
            gru9_hak_clean::kWeightHH,
            gru9_hak_clean::kWeightHHSize,
            gru9_hak_clean::kBias,
            gru9_hak_clean::kBiasSize,
            gru9_hak_clean::kDenseW,
            gru9_hak_clean::kDenseWSize,
            gru9_hak_clean::kDenseB,
            gru9_hak_clean::kDenseBSize,
            gru9_hak_clean::kLevelAdjust,
        },
        // 5: Bassman
        {
            gru9_bassman::kName,
            gru9_bassman::kHiddenSize,
            gru9_bassman::kWeightIH,
            gru9_bassman::kWeightIHSize,
            gru9_bassman::kWeightHH,
            gru9_bassman::kWeightHHSize,
            gru9_bassman::kBias,
            gru9_bassman::kBiasSize,
            gru9_bassman::kDenseW,
            gru9_bassman::kDenseWSize,
            gru9_bassman::kDenseB,
            gru9_bassman::kDenseBSize,
            gru9_bassman::kLevelAdjust,
        },
        // 6: EVH 5150
        {
            gru9_5150::kName,
            gru9_5150::kHiddenSize,
            gru9_5150::kWeightIH,
            gru9_5150::kWeightIHSize,
            gru9_5150::kWeightHH,
            gru9_5150::kWeightHHSize,
            gru9_5150::kBias,
            gru9_5150::kBiasSize,
            gru9_5150::kDenseW,
            gru9_5150::kDenseWSize,
            gru9_5150::kDenseB,
            gru9_5150::kDenseBSize,
            gru9_5150::kLevelAdjust,
        },
        // 7: Splawn
        {
            gru9_splawn::kName,
            gru9_splawn::kHiddenSize,
            gru9_splawn::kWeightIH,
            gru9_splawn::kWeightIHSize,
            gru9_splawn::kWeightHH,
            gru9_splawn::kWeightHHSize,
            gru9_splawn::kBias,
            gru9_splawn::kBiasSize,
            gru9_splawn::kDenseW,
            gru9_splawn::kDenseWSize,
            gru9_splawn::kDenseB,
            gru9_splawn::kDenseBSize,
            gru9_splawn::kLevelAdjust,
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
