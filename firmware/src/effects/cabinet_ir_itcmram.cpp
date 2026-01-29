#include "effects/cabinet_ir.h"

// ITCMRAM placement for zero-wait-state execution
#if defined(DAISY_SEED_BUILD)
#include "effects/custom_gru9.h" // For ITCMRAM_CODE macro
#else
#define ITCMRAM_CODE
#endif

/**
 * Cabinet IR convolution inner loop — placed in ITCMRAM for zero-wait-state
 * execution on Daisy Seed.
 *
 * This is a tight 128-tap MAC loop (~1,280 MACs/sample at 48kHz).
 * Running from QSPI flash incurs cache miss penalties; ITCMRAM eliminates them.
 *
 * noinline prevents LTO from inlining this into a QSPI-flash caller,
 * which would defeat the ITCMRAM placement.
 */
ITCMRAM_CODE __attribute__((noinline))
float CabinetIR_Convolve(const float *inputBuf, int inputIdx,
                          const float *irBuf, int irLen, int maxLen)
{
    float sum = 0.0f;
    int bufIdx = inputIdx;
    for (int i = 0; i < irLen; ++i)
    {
        sum += inputBuf[bufIdx] * irBuf[i];
        if (--bufIdx < 0)
            bufIdx = maxLen - 1;
    }
    return sum;
}
