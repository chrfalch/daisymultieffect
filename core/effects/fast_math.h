#pragma once

/**
 * FastMath - Centralized fast math utilities for audio DSP
 *
 * Provides optimized implementations of common math functions used in audio effects.
 * Toggle FAST_MATH_USE_CUSTOM to switch between:
 *   1 = Custom IEEE 754 bit-twiddling (~5x faster than std math)
 *   0 = DaisySP library functions (for comparison/validation)
 *
 * Features:
 * - Fast log2/pow2 approximations
 * - Fast dB to/from linear conversions
 * - 256-entry sine lookup table with linear interpolation
 * - Fast sin/cos/tan for LFO and filter calculations
 * - One-pole filter (fonepole) wrapper
 */

#ifndef FAST_MATH_USE_CUSTOM
#define FAST_MATH_USE_CUSTOM 1
#endif

#include <cstdint>
#include <cmath>

// Include DaisySP utilities when using library fallbacks (firmware only)
#if !FAST_MATH_USE_CUSTOM && defined(DAISY_SEED_BUILD)
#include "Utility/dsp.h"
#endif

namespace FastMath
{

    // ============================================================================
    // Constants
    // ============================================================================

    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    constexpr float kHalfPi = 0.5f * kPi;

    // dB conversion constants
    // For log2-based: dB = 6.0206 * log2(x), so log2 to dB multiply by 6.0206
    constexpr float kLog2ToDb = 6.0205999132796239f; // 20 / log2(10)
    // For dB to log2: log2(x) = dB / 6.0206
    constexpr float kDbToLog2 = 0.16609640474436813f; // log2(10) / 20

    // ============================================================================
    // Sine Lookup Table (256 entries for one full period)
    // ============================================================================

    constexpr int kSineTableSize = 256;
    constexpr float kSineTableScale = static_cast<float>(kSineTableSize);

    // Pre-computed sine table for one full period [0, 2π)
    // 256 entries + 1 extra for interpolation wrap
#if defined(DAISY_SEED_BUILD)
    // Firmware: table in DTCMRAM for zero-wait-state access (defined in fast_math_dtcmram.cpp)
    extern const float kSineTableData[kSineTableSize + 1];
    inline const float *GetSineTable() { return kSineTableData; }
#else
    // VST/Desktop: inline static table
    inline const float *GetSineTable()
    {
        static const float table[kSineTableSize + 1] = {
            0.000000000f, 0.024541229f, 0.049067674f, 0.073564564f, 0.098017140f,
            0.122410675f, 0.146730474f, 0.170961889f, 0.195090322f, 0.219101240f,
            0.242980180f, 0.266712757f, 0.290284677f, 0.313681740f, 0.336889853f,
            0.359895037f, 0.382683432f, 0.405241314f, 0.427555093f, 0.449611330f,
            0.471396737f, 0.492898192f, 0.514102744f, 0.534997620f, 0.555570233f,
            0.575808191f, 0.595699304f, 0.615231591f, 0.634393284f, 0.653172843f,
            0.671558955f, 0.689540545f, 0.707106781f, 0.724247083f, 0.740951125f,
            0.757208847f, 0.773010453f, 0.788346428f, 0.803207531f, 0.817584813f,
            0.831469612f, 0.844853565f, 0.857728610f, 0.870086991f, 0.881921264f,
            0.893224301f, 0.903989293f, 0.914209756f, 0.923879533f, 0.932992799f,
            0.941544065f, 0.949528181f, 0.956940336f, 0.963776066f, 0.970031253f,
            0.975702130f, 0.980785280f, 0.985277642f, 0.989176510f, 0.992479535f,
            0.995184727f, 0.997290457f, 0.998795456f, 0.999698819f, 1.000000000f,
            0.999698819f, 0.998795456f, 0.997290457f, 0.995184727f, 0.992479535f,
            0.989176510f, 0.985277642f, 0.980785280f, 0.975702130f, 0.970031253f,
            0.963776066f, 0.956940336f, 0.949528181f, 0.941544065f, 0.932992799f,
            0.923879533f, 0.914209756f, 0.903989293f, 0.893224301f, 0.881921264f,
            0.870086991f, 0.857728610f, 0.844853565f, 0.831469612f, 0.817584813f,
            0.803207531f, 0.788346428f, 0.773010453f, 0.757208847f, 0.740951125f,
            0.724247083f, 0.707106781f, 0.689540545f, 0.671558955f, 0.653172843f,
            0.634393284f, 0.615231591f, 0.595699304f, 0.575808191f, 0.555570233f,
            0.534997620f, 0.514102744f, 0.492898192f, 0.471396737f, 0.449611330f,
            0.427555093f, 0.405241314f, 0.382683432f, 0.359895037f, 0.336889853f,
            0.313681740f, 0.290284677f, 0.266712757f, 0.242980180f, 0.219101240f,
            0.195090322f, 0.170961889f, 0.146730474f, 0.122410675f, 0.098017140f,
            0.073564564f, 0.049067674f, 0.024541229f, 0.000000000f, -0.024541229f,
            -0.049067674f, -0.073564564f, -0.098017140f, -0.122410675f, -0.146730474f,
            -0.170961889f, -0.195090322f, -0.219101240f, -0.242980180f, -0.266712757f,
            -0.290284677f, -0.313681740f, -0.336889853f, -0.359895037f, -0.382683432f,
            -0.405241314f, -0.427555093f, -0.449611330f, -0.471396737f, -0.492898192f,
            -0.514102744f, -0.534997620f, -0.555570233f, -0.575808191f, -0.595699304f,
            -0.615231591f, -0.634393284f, -0.653172843f, -0.671558955f, -0.689540545f,
            -0.707106781f, -0.724247083f, -0.740951125f, -0.757208847f, -0.773010453f,
            -0.788346428f, -0.803207531f, -0.817584813f, -0.831469612f, -0.844853565f,
            -0.857728610f, -0.870086991f, -0.881921264f, -0.893224301f, -0.903989293f,
            -0.914209756f, -0.923879533f, -0.932992799f, -0.941544065f, -0.949528181f,
            -0.956940336f, -0.963776066f, -0.970031253f, -0.975702130f, -0.980785280f,
            -0.985277642f, -0.989176510f, -0.992479535f, -0.995184727f, -0.997290457f,
            -0.998795456f, -0.999698819f, -1.000000000f, -0.999698819f, -0.998795456f,
            -0.997290457f, -0.995184727f, -0.992479535f, -0.989176510f, -0.985277642f,
            -0.980785280f, -0.975702130f, -0.970031253f, -0.963776066f, -0.956940336f,
            -0.949528181f, -0.941544065f, -0.932992799f, -0.923879533f, -0.914209756f,
            -0.903989293f, -0.893224301f, -0.881921264f, -0.870086991f, -0.857728610f,
            -0.844853565f, -0.831469612f, -0.817584813f, -0.803207531f, -0.788346428f,
            -0.773010453f, -0.757208847f, -0.740951125f, -0.724247083f, -0.707106781f,
            -0.689540545f, -0.671558955f, -0.653172843f, -0.634393284f, -0.615231591f,
            -0.595699304f, -0.575808191f, -0.555570233f, -0.534997620f, -0.514102744f,
            -0.492898192f, -0.471396737f, -0.449611330f, -0.427555093f, -0.405241314f,
            -0.382683432f, -0.359895037f, -0.336889853f, -0.313681740f, -0.290284677f,
            -0.266712757f, -0.242980180f, -0.219101240f, -0.195090322f, -0.170961889f,
            -0.146730474f, -0.122410675f, -0.098017140f, -0.073564564f, -0.049067674f,
            -0.024541229f, 0.000000000f
        };
        return table;
    }
#endif

    // ============================================================================
    // Fast Trigonometric Functions
    // ============================================================================

    /**
     * Fast sine using lookup table with linear interpolation
     * @param phase Normalized phase [0, 1] representing [0, 2π]
     * @return Sine value in [-1, 1]
     */
    inline float fastSin(float phase)
    {
        // Wrap phase to [0, 1)
        phase = phase - static_cast<int>(phase);
        if (phase < 0.0f)
            phase += 1.0f;

        // Scale to table index
        float indexF = phase * kSineTableScale;
        int index = static_cast<int>(indexF);
        float frac = indexF - static_cast<float>(index);

        // Ensure index is in valid range
        index &= (kSineTableSize - 1);

        // Linear interpolation between adjacent table entries
        const float *table = GetSineTable();
        return table[index] + frac * (table[index + 1] - table[index]);
    }

    /**
     * Fast cosine: cos(x) = sin(x + π/2) = sin(phase + 0.25)
     * @param phase Normalized phase [0, 1] representing [0, 2π]
     * @return Cosine value in [-1, 1]
     */
    inline float fastCos(float phase)
    {
        return fastSin(phase + 0.25f);
    }

    /**
     * Fast sine for radians input
     * @param radians Angle in radians
     * @return Sine value in [-1, 1]
     */
    inline float fastSinRad(float radians)
    {
        return fastSin(radians * (1.0f / kTwoPi));
    }

    /**
     * Fast cosine for radians input
     * @param radians Angle in radians
     * @return Cosine value in [-1, 1]
     */
    inline float fastCosRad(float radians)
    {
        return fastCos(radians * (1.0f / kTwoPi));
    }

    /**
     * Fast tangent using sin/cos ratio
     * @param phase Normalized phase [0, 1] representing [0, 2π]
     * @return Tangent value (unbounded, but accurate for typical audio use)
     *
     * Note: Returns large values near π/2 and 3π/2 (phase 0.25 and 0.75)
     * For filter coefficient calculation, input is typically far from these poles.
     */
    inline float fastTan(float phase)
    {
        float s = fastSin(phase);
        float c = fastCos(phase);
        // Avoid division by zero - return large value at poles
        if (c > -1e-6f && c < 1e-6f)
            return (s >= 0.0f) ? 1e6f : -1e6f;
        return s / c;
    }

    /**
     * Fast tangent for radians input
     * @param radians Angle in radians
     * @return Tangent value
     */
    inline float fastTanRad(float radians)
    {
        return fastTan(radians * (1.0f / kTwoPi));
    }

    // ============================================================================
    // Fast Logarithm and Power Functions
    // ============================================================================

#if FAST_MATH_USE_CUSTOM

    /**
     * Ultra-fast log2 approximation using IEEE 754 float bit representation
     * Accuracy: ~0.1dB for audio applications
     * Speed: ~5x faster than logf()
     */
    inline float fastLog2(float x)
    {
        union
        {
            float f;
            int32_t i;
        } vx = {x};
        float y = static_cast<float>(vx.i);
        y *= 1.1920928955078125e-7f; // 1/(2^23)
        return y - 126.94269504f;
    }

    /**
     * Ultra-fast pow2 approximation using IEEE 754 float bit representation
     * Accuracy: ~0.1dB for audio applications
     * Speed: ~5x faster than expf()
     */
    inline float fastPow2(float p)
    {
        // Clamp to avoid overflow/underflow
        float clipp = (p < -126.0f) ? -126.0f : ((p > 126.0f) ? 126.0f : p);
        union
        {
            int32_t i;
            float f;
        } v;
        v.i = static_cast<int32_t>((1 << 23) * (clipp + 126.94269504f));
        return v.f;
    }

    /**
     * Fast exp approximation: exp(x) = pow2(x / ln(2))
     */
    inline float fastExp(float x)
    {
        return fastPow2(x * 1.442695040888963f); // 1/ln(2)
    }

#else // Use DaisySP library functions

#if defined(DAISY_SEED_BUILD)

    inline float fastLog2(float x)
    {
        return daisysp::fastlog2f(x);
    }

    inline float fastPow2(float p)
    {
        // DaisySP doesn't have pow2, but has pow10f
        // pow2(x) = pow10(x * log10(2)) = pow10(x * 0.30103)
        return daisysp::pow10f(p * 0.30102999566398119521373889472449f);
    }

    inline float fastExp(float x)
    {
        // exp(x) = pow10(x / ln(10)) = pow10(x * 0.4343)
        return daisysp::pow10f(x * 0.43429448190325182765112891891661f);
    }

#else // Non-Daisy builds (VST) - use standard library

    inline float fastLog2(float x)
    {
        return std::log2(x);
    }

    inline float fastPow2(float p)
    {
        return std::exp2(p);
    }

    inline float fastExp(float x)
    {
        return std::exp(x);
    }

#endif // DAISY_SEED_BUILD

#endif // FAST_MATH_USE_CUSTOM

    // ============================================================================
    // dB Conversion Functions
    // ============================================================================

    /**
     * Fast dB to linear amplitude conversion
     * @param dB Decibel value
     * @return Linear amplitude (pow(10, dB/20))
     */
    inline float fastDbToLin(float dB)
    {
        // pow(10, dB/20) = pow2(dB / 6.0206)
        return fastPow2(dB * kDbToLog2);
    }

    /**
     * Fast linear amplitude to dB conversion
     * @param lin Linear amplitude (must be > 0)
     * @return Decibel value (20 * log10(lin))
     */
    inline float fastLinToDb(float lin)
    {
        // 20 * log10(x) = 6.0206 * log2(x)
        return fastLog2(lin) * kLog2ToDb;
    }

    // ============================================================================
    // Utility Functions
    // ============================================================================

    /**
     * One-pole lowpass filter (exponential smoothing)
     * @param out Output value (passed by reference, retains state between calls)
     * @param in Input value
     * @param coeff Filter coefficient: coeff = 1.0 / (time * sampleRate)
     */
    inline void fonepole(float &out, float in, float coeff)
    {
        out += coeff * (in - out);
    }

    /**
     * Calculate one-pole coefficient from time constant
     * @param timeSeconds Time constant in seconds
     * @param sampleRate Sample rate in Hz
     * @return Coefficient for use with fonepole()
     */
    inline float calcOnePoleCoeff(float timeSeconds, float sampleRate)
    {
        if (timeSeconds <= 0.0f || sampleRate <= 0.0f)
            return 1.0f;
        return 1.0f / (timeSeconds * sampleRate);
    }

    /**
     * Calculate exponential envelope coefficient (for attack/release)
     * @param timeSeconds Time constant in seconds
     * @param sampleRate Sample rate in Hz
     * @return Coefficient for envelope follower: out = coeff * out + (1-coeff) * in
     */
    inline float calcEnvelopeCoeff(float timeSeconds, float sampleRate)
    {
        if (timeSeconds <= 0.0f || sampleRate <= 0.0f)
            return 0.0f;
        return fastExp(-1.0f / (timeSeconds * sampleRate));
    }

    /**
     * Fast floating-point clamp
     */
    inline float fclamp(float x, float minVal, float maxVal)
    {
        return (x < minVal) ? minVal : ((x > maxVal) ? maxVal : x);
    }

    /**
     * Fast floating-point min
     */
    inline float fmin(float a, float b)
    {
        return (a < b) ? a : b;
    }

    /**
     * Fast floating-point max
     */
    inline float fmax(float a, float b)
    {
        return (a > b) ? a : b;
    }

    /**
     * Fast absolute value
     */
    inline float fabs(float x)
    {
        return (x < 0.0f) ? -x : x;
    }

} // namespace FastMath
