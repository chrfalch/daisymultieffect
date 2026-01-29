#pragma once
/**
 * Custom GRU-9 Implementation for Daisy Seed Firmware
 *
 * Hand-optimized GRU with hidden_size=9, input_size=1, output_size=1.
 * Replaces RTNeural on firmware for zero-template-bloat, ITCMRAM-placed execution.
 *
 * The forward pass and fast math functions are placed in ITCMRAM (64KB, zero-wait-state)
 * to avoid QSPI flash cache miss penalties (~10-20 cycles per miss).
 *
 * Activation functions use 512-entry lookup tables with linear interpolation,
 * stored in DTCMRAM for zero-wait-state data access. This avoids the expensive
 * VDIV.F32 instruction (14+ cycles) in the Padé approximant.
 *
 * Weight layout follows RTNeural GRU convention:
 *   Gate order: reset (r), update (z), new (n)
 *   weightIH[27]: input-to-hidden [1 x 3*9], gate-interleaved
 *   weightHH[9*27]: hidden-to-hidden [9 x 3*9], gate-interleaved
 *   bias[2*27]: two bias vectors [2 x 3*9]
 *   denseW[9]: output dense weights
 *   denseB[1]: output dense bias
 */

#include <cstdint>
#include <cstddef>

// ITCMRAM/DTCMRAM placement for zero-wait-state execution on Daisy Seed
#if defined(DAISY_SEED_BUILD)
#define ITCMRAM_CODE __attribute__((section(".itcmram_text")))
#define DTCMRAM_DATA __attribute__((section(".dtcmram_bss")))
#define DTCMRAM_CONST __attribute__((section(".dtcmram_data")))
#else
#define ITCMRAM_CODE
#define DTCMRAM_DATA
#define DTCMRAM_CONST
#endif

/**
 * Pre-split weight storage for GRU-9.
 * Weights are split by gate at load time to avoid per-sample index arithmetic.
 */
struct CustomGRU9Weights
{
    // Input-to-hidden weights (scalar multiply since input_size=1)
    float Wr[9]; // Reset gate
    float Wz[9]; // Update gate
    float Wn[9]; // New gate

    // Hidden-to-hidden weights [9][9] per gate
    float Ur[9][9]; // Reset gate recurrent
    float Uz[9][9]; // Update gate recurrent
    float Un[9][9]; // New gate recurrent

    // Biases
    float br[9];  // Reset gate bias (input)
    float bz[9];  // Update gate bias (input)
    float bn0[9]; // New gate bias (input)
    float br1[9]; // Reset gate bias (recurrent)
    float bz1[9]; // Update gate bias (recurrent)
    float bn1[9]; // New gate bias (recurrent)

    // Pre-combined biases (br+br1, bz+bz1) — computed once at load time
    float br_c[9]; // Reset: br + br1
    float bz_c[9]; // Update: bz + bz1

    // Dense output layer
    float denseW[9];
    float denseB;
};

/**
 * Custom GRU-9 neural network for amp modeling.
 * Self-contained, no RTNeural dependency.
 */
class CustomGRU9
{
public:
    /**
     * Reset hidden state to zero.
     */
    void reset()
    {
        for (int i = 0; i < 9; ++i)
            h_[i] = 0.0f;
    }

    /**
     * Load weights from raw float arrays (RTNeural GRU convention).
     *
     * @param weightIH  Input-to-hidden [27]: [Wr0..8, Wz0..8, Wn0..8]
     * @param weightHH  Hidden-to-hidden [9*27] row-major
     * @param bias      Biases [2*27]: input biases then recurrent biases
     * @param denseW    Dense output weights [9]
     * @param denseB    Dense output bias (single float)
     */
    void loadWeights(const float *weightIH, const float *weightHH,
                     const float *bias, const float *denseW, float denseB)
    {
        // Split input-to-hidden weights by gate.
        // RTNeural convention: [z0..z8, r0..r8, n0..n8] (update, reset, new)
        for (int i = 0; i < 9; ++i)
        {
            w_.Wz[i] = weightIH[i];
            w_.Wr[i] = weightIH[9 + i];
            w_.Wn[i] = weightIH[18 + i];
        }

        // Split hidden-to-hidden weights by gate
        // weightHH is [9][27] row-major: row h has [Uz_h[0..8], Ur_h[0..8], Un_h[0..8]]
        for (int h = 0; h < 9; ++h)
        {
            for (int i = 0; i < 9; ++i)
            {
                w_.Uz[h][i] = weightHH[h * 27 + i];
                w_.Ur[h][i] = weightHH[h * 27 + 9 + i];
                w_.Un[h][i] = weightHH[h * 27 + 18 + i];
            }
        }

        // Split biases: bias[0][27] = input biases, bias[1][27] = recurrent biases
        // RTNeural convention: [z, r, n] gate order
        for (int i = 0; i < 9; ++i)
        {
            w_.bz[i]  = bias[i];
            w_.br[i]  = bias[9 + i];
            w_.bn0[i] = bias[18 + i];
            w_.bz1[i] = bias[27 + i];
            w_.br1[i] = bias[27 + 9 + i];
            w_.bn1[i] = bias[27 + 18 + i];
        }

        // Dense output layer
        for (int i = 0; i < 9; ++i)
            w_.denseW[i] = denseW[i];
        w_.denseB = denseB;

        // Pre-combine biases that are always summed together
        for (int i = 0; i < 9; ++i)
        {
            w_.br_c[i] = w_.br[i] + w_.br1[i];
            w_.bz_c[i] = w_.bz[i] + w_.bz1[i];
        }

        reset();
    }

    /**
     * Forward pass: process one input sample through GRU-9 + Dense.
     * Returns raw model output (no residual — caller applies that).
     *
     * Placed in ITCMRAM for zero-wait-state execution.
     */
    ITCMRAM_CODE float forward(float input);

private:
    // Hidden state — placed in DTCMRAM for zero-wait-state data access
    DTCMRAM_DATA static float h_[9];

    // Weights — placed in DTCMRAM for zero-wait-state data access
    DTCMRAM_DATA static CustomGRU9Weights w_;

    // =========================================================================
    // Lookup-table-based activation functions
    // =========================================================================

    // 512-entry tanh table covering [-5, 5] with linear interpolation.
    // Stored in DTCMRAM for zero-wait-state reads (~10 cycles vs ~30 for Padé).
    // Table has 513 entries (512 intervals + 1 endpoint for interpolation).
    static const float kTanhTable[513];
    static const float kSigmoidTable[513];

    // Tanh table parameters
    static constexpr float kTanhMin = -5.0f;
    static constexpr float kTanhMax = 5.0f;
    static constexpr float kTanhRange = kTanhMax - kTanhMin; // 10.0
    static constexpr float kTanhScale = 512.0f / kTanhRange; // 51.2
    static constexpr int kTanhTableSize = 512;

    // Sigmoid table parameters
    static constexpr float kSigmoidMin = -10.0f;
    static constexpr float kSigmoidMax = 10.0f;
    static constexpr float kSigmoidRange = kSigmoidMax - kSigmoidMin; // 20.0
    static constexpr float kSigmoidScale = 512.0f / kSigmoidRange;    // 25.6
    static constexpr int kSigmoidTableSize = 512;

    /**
     * Fast tanh via 512-entry lookup table with linear interpolation.
     * Accuracy: ~1e-4 (sufficient for neural amp modeling).
     * Cost: ~10 cycles (2 loads + 1 fmaf + offset/scale arithmetic).
     */
    static inline float fast_tanh(float x)
    {
        if (x <= kTanhMin) return -1.0f;
        if (x >= kTanhMax) return 1.0f;
        float indexF = (x - kTanhMin) * kTanhScale;
        int idx = static_cast<int>(indexF);
        float frac = indexF - static_cast<float>(idx);
        return kTanhTable[idx] + frac * (kTanhTable[idx + 1] - kTanhTable[idx]);
    }

    /**
     * Fast sigmoid via 512-entry lookup table with linear interpolation.
     * Direct lookup avoids the 0.5 + 0.5*tanh(x/2) indirection (saves ~54 cycles/sample).
     * Accuracy: ~1e-4 (sufficient for neural amp modeling).
     */
    static inline float fast_sigmoid(float x)
    {
        if (x <= kSigmoidMin) return 0.0f;
        if (x >= kSigmoidMax) return 1.0f;
        float indexF = (x - kSigmoidMin) * kSigmoidScale;
        int idx = static_cast<int>(indexF);
        float frac = indexF - static_cast<float>(idx);
        return kSigmoidTable[idx] + frac * (kSigmoidTable[idx + 1] - kSigmoidTable[idx]);
    }
};

// Static members and forward() defined in custom_gru9.cpp
