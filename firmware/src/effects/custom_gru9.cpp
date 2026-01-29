#include "effects/custom_gru9.h"

// Static member definitions
DTCMRAM_DATA float CustomGRU9::h_[9] = {};
CustomGRU9Weights CustomGRU9::w_ = {};

/**
 * GRU-9 forward pass — placed in ITCMRAM for zero-wait-state execution.
 *
 * This is in a .cpp file (not header) to ensure:
 * 1. Single definition — no ODR issues across translation units
 * 2. Proper ITCMRAM section placement — noinline prevents LTO from
 *    inlining this into QSPI flash callers
 * 3. The function is called via a branch to ITCMRAM address, executing
 *    entirely from zero-wait-state memory
 */
ITCMRAM_CODE __attribute__((noinline))
float CustomGRU9::forward(float input)
{
    // Phase 1: Matrix-vector products (j-outer for contiguous weight access).
    // Ur[j][0..8] is a contiguous row, so the inner i-loop streams through
    // memory sequentially instead of striding by 9.
    float dr[9] = {0}, dz[9] = {0}, dn[9] = {0};
    for (int j = 0; j < 9; ++j)
    {
        float hj = h_[j];
        const float *ur_row = w_.Ur[j];
        const float *uz_row = w_.Uz[j];
        const float *un_row = w_.Un[j];
        // 3x3 unroll: 9 = 3×3, process 3 outputs per iteration
        // with 3 independent memory streams per gate
        for (int i = 0; i < 9; i += 3)
        {
            dr[i]     += ur_row[i]     * hj;
            dr[i + 1] += ur_row[i + 1] * hj;
            dr[i + 2] += ur_row[i + 2] * hj;
            dz[i]     += uz_row[i]     * hj;
            dz[i + 1] += uz_row[i + 1] * hj;
            dz[i + 2] += uz_row[i + 2] * hj;
            dn[i]     += un_row[i]     * hj;
            dn[i + 1] += un_row[i + 1] * hj;
            dn[i + 2] += un_row[i + 2] * hj;
        }
    }

    // Phase 2: Activations + hidden state update + dense output.
    // Uses pre-combined biases (br_c = br + br1, bz_c = bz + bz1)
    // to eliminate 18 adds per sample.
    float output = w_.denseB;
    for (int i = 0; i < 9; ++i)
    {
        float r = fast_sigmoid(w_.Wr[i] * input + dr[i] + w_.br_c[i]);
        float z = fast_sigmoid(w_.Wz[i] * input + dz[i] + w_.bz_c[i]);
        float n = fast_tanh(w_.Wn[i] * input + w_.bn0[i] + r * (dn[i] + w_.bn1[i]));
        h_[i] = (1.0f - z) * n + z * h_[i];
        output += w_.denseW[i] * h_[i];
    }

    return output;
}
