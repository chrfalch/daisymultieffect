/**
 * @file nam_wavenet.h
 * @brief NAM WaveNet "Pico" model implementation for Daisy Seed firmware
 *
 * This is a firmware-specific implementation using the Mercury project's
 * WaveNet architecture with Eigen backend. Uses "Pico" models which have:
 * - 2 channels (smaller than standard NAM's 4 channels)
 * - 20 dilation layers (7 + 13)
 * - ~500 weights per model
 *
 * Based on GuitarML/Mercury: https://github.com/GuitarML/Mercury
 */
#pragma once

#if defined(DAISY_SEED_BUILD) && defined(HAS_RTNEURAL) && HAS_RTNEURAL

#include <RTNeural/RTNeural.h>
#include "effects/embedded/wavenet/wavenet_model.hpp"

namespace nam_wavenet
{

    /**
     * NAMMathsProvider - Fast tanh approximation for WaveNet
     *
     * Uses math_approx polynomial approximation optimized for
     * Cortex-M7 with FPU. Faster than std::tanh while maintaining
     * acceptable accuracy for audio processing.
     */
    struct NAMMathsProvider
    {
#if RTNEURAL_USE_EIGEN
        template <typename Matrix>
        static auto tanh(const Matrix &x)
        {
            // Polynomial approximation: tanh(x) ≈ x * (1 + 0.183 * x²) / sqrt(1 + (x * (1 + 0.183 * x²))²)
            // Accurate to ~3 decimal places, much faster than std::tanh
            const auto x_poly = x.array() * (1.0f + 0.183428244899f * x.array().square());
            return x_poly.array() * (x_poly.array().square() + 1.0f).array().rsqrt();
        }
#endif
    };

    /**
     * NAM "Pico" WaveNet model type definition
     *
     * Architecture matches Mercury's model_pico.json:
     * - Layer 1: input=1, condition=1, head=2, channels=2, kernel=3, dilations=[1,2,4,8,16,32,64]
     * - Layer 2: input=2, condition=1, head=1, channels=2, kernel=3, dilations=[128,256,512,1,2,4,8,16,32,64,128,256,512]
     */
    using Dilations1 = wavenet::Dilations<1, 2, 4, 8, 16, 32, 64>;
    using Dilations2 = wavenet::Dilations<128, 256, 512, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512>;

    using LayerArray1 = wavenet::Layer_Array<float, 1, 1, 2, 2, 3, Dilations1, false, NAMMathsProvider>;
    using LayerArray2 = wavenet::Layer_Array<float, 2, 1, 1, 2, 3, Dilations2, true, NAMMathsProvider>;

    using PicoModel = wavenet::Wavenet_Model<float, 1, LayerArray1, LayerArray2>;

    /**
     * Model info structure for embedded NAM Pico models
     */
    struct NamPicoModelInfo
    {
        const char *name;
        const float *weights;
        size_t weightCount;
    };

    // Forward declaration of model count - defined in nam_model_weights.h
    constexpr size_t kNumNamPicoModels = 9;

    /**
     * Get model info by index
     */
    const NamPicoModelInfo *GetNamPicoModel(size_t index);

    /**
     * Initialize the NAM Pico model weights from embedded data
     */
    void InitNamPicoModels();

} // namespace nam_wavenet

#endif // DAISY_SEED_BUILD && HAS_RTNEURAL
