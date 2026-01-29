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

// ITCMRAM/DTCMRAM placement (disabled - using standard linker script)
#define ITCMRAM_CODE
#define DTCMRAM_DATA

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

    // Weights — kept in regular SRAM (read once per sample, cached well)
    static CustomGRU9Weights w_;

    /**
     * Fast tanh using Padé approximant.
     * Accurate to ~1e-5 for |x| < 4.95, clamps to ±1 beyond.
     * ~5-8 cycles vs ~60 for std::tanh on Cortex-M7.
     */
    static inline float fast_tanh(float x)
    {
        if (x > 4.95f) return 1.0f;
        if (x < -4.95f) return -1.0f;
        float x2 = x * x;
        return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) /
               (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
    }

    /**
     * Fast sigmoid via tanh: sigmoid(x) = 0.5 + 0.5 * tanh(x * 0.5)
     */
    static inline float fast_sigmoid(float x)
    {
        return 0.5f + 0.5f * fast_tanh(x * 0.5f);
    }
};

// Static members and forward() defined in custom_gru9.cpp
