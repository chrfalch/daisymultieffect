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
    float r[9], z[9], n[9];

    // Compute gate pre-activations
    for (int i = 0; i < 9; ++i)
    {
        // Recurrent contributions: RTNeural stores U transposed as [hidden][gate]
        // So output[i] = sum_j(U[j][i] * h[j])
        float dot_r = 0.0f, dot_z = 0.0f, dot_n = 0.0f;
        for (int j = 0; j < 9; ++j)
        {
            float hj = h_[j];
            dot_r += w_.Ur[j][i] * hj;
            dot_z += w_.Uz[j][i] * hj;
            dot_n += w_.Un[j][i] * hj;
        }

        // Reset gate: r = sigmoid(Wr * input + dot(Ur, h) + br + br1)
        r[i] = fast_sigmoid(w_.Wr[i] * input + dot_r + w_.br[i] + w_.br1[i]);

        // Update gate: z = sigmoid(Wz * input + dot(Uz, h) + bz + bz1)
        z[i] = fast_sigmoid(w_.Wz[i] * input + dot_z + w_.bz[i] + w_.bz1[i]);

        // New gate candidate: n = tanh(Wn * input + bn0 + r * (dot(Un, h) + bn1))
        n[i] = fast_tanh(w_.Wn[i] * input + w_.bn0[i] + r[i] * (dot_n + w_.bn1[i]));
    }

    // Update hidden state: h = (1 - z) * n + z * h_prev
    for (int i = 0; i < 9; ++i)
        h_[i] = (1.0f - z[i]) * n[i] + z[i] * h_[i];

    // Dense output: dot(denseW, h) + denseB
    float output = w_.denseB;
    for (int i = 0; i < 9; ++i)
        output += w_.denseW[i] * h_[i];

    return output;
}
