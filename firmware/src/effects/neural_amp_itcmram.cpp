/**
 * ITCMRAM-placed ProcessStereo for NeuralAmpEffect.
 *
 * Moves the entire per-sample hot path (mono sum, input gain, GRU forward,
 * residual, level adjust, 3-band biquad EQ, output gain, clamp) into
 * ITCMRAM to avoid QSPI flash cache miss penalties.
 *
 * Follows the established pattern in effects_itcmram.cpp.
 * Inline helpers (FastMath::fastDbToLin, processBiquad, etc.) are inlined
 * INTO this ITCMRAM function, so they also execute from zero-wait-state memory.
 */

#if defined(DAISY_SEED_BUILD)
#define ITCMRAM_CODE __attribute__((section(".itcmram_text")))
#else
#define ITCMRAM_CODE
#endif

#include "effects/neural_amp.h"

/**
 * ITCMRAM-placed per-sample processing for neural amp.
 * Called from NeuralAmpEffect::ProcessStereo().
 *
 * All inline helpers (FastMath::*, processBiquad, fclamp) are inlined into
 * this function, ensuring the entire sample processing chain runs from
 * zero-wait-state ITCMRAM.
 */
ITCMRAM_CODE __attribute__((noinline))
void NeuralAmpEffect::ProcessSampleITCMRAM(float &l, float &r)
{
    // Convert to mono for neural network (amp models are mono)
    float mono = 0.5f * (l + r);

    // Apply input gain (-20dB to +20dB range)
    float inGain = FastMath::fastDbToLin((inputGain_ - 0.5f) * 40.0f);
    mono *= inGain;

    // GRU-9 forward pass + residual connection (Mars approach)
    if (modelLoaded_)
    {
        mono = model_.forward(mono) + mono;
        mono *= levelAdjust_;
    }
    else
    {
        // Fallback soft clipping when no model loaded
        mono = std::tanh(mono * 2.0f) * 0.7f;
    }

    // Apply 3-band EQ (skip if at neutral)
    float output = mono;

    if (FastMath::fabs(bass_ - 0.5f) > 0.01f)
        output = processBiquad(bassState_, bassCoeffs_, output);

    if (FastMath::fabs(mid_ - 0.5f) > 0.01f)
        output = processBiquad(midState_, midCoeffs_, output);

    if (FastMath::fabs(treble_ - 0.5f) > 0.01f)
        output = processBiquad(trebleState_, trebleCoeffs_, output);

    // Apply output gain (-20dB to +20dB range)
    float outGain = FastMath::fastDbToLin((outputGain_ - 0.5f) * 40.0f);
    output *= outGain;

    // Soft limit to prevent clipping
    output = fclamp(output, -1.5f, 1.5f);

    // Output to stereo
    l = r = output;
}
