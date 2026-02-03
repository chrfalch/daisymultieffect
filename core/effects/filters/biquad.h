#pragma once

#include <cmath>

/**
 * Generic 2nd-order IIR filter (Biquad)
 * 
 * Direct Form I implementation for cache-friendly processing.
 * Supports standard filter types: lowpass, highpass, bandpass, notch, etc.
 * 
 * Transfer function:
 *         b0 + b1*z^-1 + b2*z^-2
 * H(z) = -------------------------
 *         a0 + a1*z^-1 + a2*z^-2
 * 
 * Difference equation (normalized by a0):
 * y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
 *                      - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]
 */
struct Biquad
{
    // Normalized coefficients (pre-divided by a0)
    float b0_ = 1.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;

    // State variables (Direct Form I)
    float x1_ = 0.0f; // x[n-1]
    float x2_ = 0.0f; // x[n-2]
    float y1_ = 0.0f; // y[n-1]
    float y2_ = 0.0f; // y[n-2]

    /**
     * Process a single sample through the biquad filter.
     * Inline for hot path performance.
     */
    inline float Process(float in)
    {
        // Direct Form I: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
        float out = b0_ * in + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

        // Update state
        x2_ = x1_;
        x1_ = in;
        y2_ = y1_;
        y1_ = out;

        return out;
    }

    /**
     * Reset filter state (clear history).
     */
    void Reset()
    {
        x1_ = x2_ = 0.0f;
        y1_ = y2_ = 0.0f;
    }

    /**
     * Configure as Butterworth lowpass filter.
     * 
     * @param fc Cutoff frequency (Hz)
     * @param fs Sample rate (Hz)
     */
    void SetLowpass(float fc, float fs)
    {
        // Butterworth lowpass: Q = 1/sqrt(2) ≈ 0.7071
        const float Q = 0.70710678118f;
        
        float w0 = 2.0f * 3.14159265358979323846f * fc / fs;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        // Unnormalized coefficients
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha;
        float b0 = (1.0f - cosw0) * 0.5f;
        float b1 = 1.0f - cosw0;
        float b2 = (1.0f - cosw0) * 0.5f;

        // Normalize by a0
        float a0inv = 1.0f / a0;
        b0_ = b0 * a0inv;
        b1_ = b1 * a0inv;
        b2_ = b2 * a0inv;
        a1_ = a1 * a0inv;
        a2_ = a2 * a0inv;
    }

    /**
     * Configure as Butterworth highpass filter.
     * 
     * @param fc Cutoff frequency (Hz)
     * @param fs Sample rate (Hz)
     */
    void SetHighpass(float fc, float fs)
    {
        // Butterworth highpass: Q = 1/sqrt(2) ≈ 0.7071
        const float Q = 0.70710678118f;
        
        float w0 = 2.0f * 3.14159265358979323846f * fc / fs;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        // Unnormalized coefficients
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha;
        float b0 = (1.0f + cosw0) * 0.5f;
        float b1 = -(1.0f + cosw0);
        float b2 = (1.0f + cosw0) * 0.5f;

        // Normalize by a0
        float a0inv = 1.0f / a0;
        b0_ = b0 * a0inv;
        b1_ = b1 * a0inv;
        b2_ = b2 * a0inv;
        a1_ = a1 * a0inv;
        a2_ = a2 * a0inv;
    }
};
