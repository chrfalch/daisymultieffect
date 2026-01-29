#include "effects/cabinet_ir.h"

// ITCMRAM placement for zero-wait-state execution
#if defined(DAISY_SEED_BUILD)
#include "effects/custom_gru9.h" // For ITCMRAM_CODE macro
#else
#define ITCMRAM_CODE
#endif

/**
 * 4x-unrolled MAC helper for a linear segment.
 * Uses 4 independent accumulators to break FPU pipeline dependencies
 * (Cortex-M7 FPU has ~3-cycle FMLA latency; 4 accumulators keep it fed).
 */
static ITCMRAM_CODE __attribute__((always_inline)) inline
float mac_segment(const float *input, int inputStart, int inputDir,
                  const float *ir, int irStart, int len)
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    int i = 0;
    int len4 = len & ~3;
    for (; i < len4; i += 4)
    {
        s0 += input[inputStart + inputDir * i]       * ir[irStart + i];
        s1 += input[inputStart + inputDir * (i + 1)] * ir[irStart + i + 1];
        s2 += input[inputStart + inputDir * (i + 2)] * ir[irStart + i + 2];
        s3 += input[inputStart + inputDir * (i + 3)] * ir[irStart + i + 3];
    }
    for (; i < len; ++i)
        s0 += input[inputStart + inputDir * i] * ir[irStart + i];
    return s0 + s1 + s2 + s3;
}

/**
 * Cabinet IR convolution inner loop — placed in ITCMRAM for zero-wait-state
 * execution on Daisy Seed.
 *
 * Splits the circular buffer traversal into two branchless linear segments,
 * each processed with a 4x-unrolled MAC loop. This eliminates the per-tap
 * wraparound branch and improves FPU pipeline utilization.
 *
 * noinline prevents LTO from inlining this into a QSPI-flash caller,
 * which would defeat the ITCMRAM placement.
 */
ITCMRAM_CODE __attribute__((noinline))
float CabinetIR_Convolve(const float *inputBuf, int inputIdx,
                          const float *irBuf, int irLen, int maxLen)
{
    // Segment 1: from inputIdx backwards toward index 0
    int seg1 = inputIdx + 1; // samples available before wrap
    if (seg1 > irLen)
        seg1 = irLen;

    float sum = mac_segment(inputBuf, inputIdx, -1, irBuf, 0, seg1);

    // Segment 2: remaining taps wrap around from end of buffer
    int seg2 = irLen - seg1;
    if (seg2 > 0)
        sum += mac_segment(inputBuf, maxLen - 1, -1, irBuf, seg1, seg2);

    return sum;
}
