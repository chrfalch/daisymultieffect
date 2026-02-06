
#pragma once
#include <cstdint>
#include <cmath>

/**
 * Tape Flutter Utility
 * 
 * Simulates analog tape wow and flutter using 1D Perlin noise with Fractal Brownian Motion (FBM).
 * Creates smooth, natural-sounding speed variations that mimic the imperfections of analog tape machines.
 * 
 * Usage in delay effects:
 * 
 * class MyDelayEffect : BaseEffect {
 *     TapeFlutter tapeFlutter_;
 *     
 *     void Init(float sr) {
 *         tapeFlutter_.Init(sr);
 *     }
 *     
 *     void ProcessStereo(float &l, float &r) {
 *         // Get tape speed modulation value (normalized around 0, range roughly -1 to +1)
 *         float speedMod = tapeFlutter_.GetTapeSpeed(
 *             0.5f,  // wow_rate (Hz) - slow modulation, typically 0.2-2 Hz
 *             3.0f,  // flutter_rate (Hz) - fast modulation, typically 2-7 Hz
 *             1.0f,  // wow_depth - scaling factor for wow effect
 *             0.5f   // flutter_depth - scaling factor for flutter (usually smaller than wow)
 *         );
 *         
 *         // Apply to delay time modulation
 *         float modulatedDelay = baseDelay + speedMod * modulationAmount;
 *         // ... rest of delay processing
 *     }
 * };
 * 
 * Reference: https://github.com/bkshepherd/DaisySeedProjects/commit/f2031eb86b56bc69d8f50d3e26b4a90e9bffbf61
 */
class TapeFlutter
{
public:
    /**
     * Initialize the tape flutter generator
     * @param sampleRate Audio sample rate in Hz
     */
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        tWow_ = 0.0f;
        tFlutter_ = 0.0f;
        
        // Initialize permutation table (duplicate for wraparound)
        for (int i = 0; i < 256; i++)
        {
            perm_[i] = perm_[i + 256] = kPerlinP[i];
        }
    }

    /**
     * Generate tape speed variation value
     * @param wowRate Rate of slow wow modulation in Hz (typically 0.2-2 Hz)
     * @param flutterRate Rate of fast flutter modulation in Hz (typically 2-7 Hz)
     * @param wowDepth Depth/amount of wow effect (scaling factor, typically 0.5-2.0)
     * @param flutterDepth Depth/amount of flutter effect (scaling factor, typically 0.2-1.0)
     * @return Combined modulation value (normalized around 0, roughly -1 to +1 range)
     */
    float GetTapeSpeed(float wowRate, float flutterRate, float wowDepth, float flutterDepth)
    {
        // Slow WOW component (2 octaves of FBM for smooth, natural variation)
        float wow = Fbm1D(tWow_, 2, 2.0f, 0.5f);

        // Fast FLUTTER component (1 octave, less complex)
        float flutter = Fbm1D(tFlutter_, 1, 2.0f, 0.5f);

        // Advance time accumulators
        tWow_ += wowRate / sampleRate_;
        tFlutter_ += flutterRate / sampleRate_;

        // Wrap time accumulators to prevent floating-point precision loss
        // Wrap at 256.0 since Perlin uses modulo 256 for lattice coordinates
        if (tWow_ >= 256.0f)
            tWow_ -= 256.0f;
        if (tFlutter_ >= 256.0f)
            tFlutter_ -= 256.0f;

        // Combine, scaled by depth
        // Flutter typically has less impact, so scale it down (0.2x)
        float speed = wowDepth * wow + (flutterDepth * 0.2f) * flutter;

        return speed;
    }

    /**
     * Reset the internal state
     */
    void Reset()
    {
        tWow_ = 0.0f;
        tFlutter_ = 0.0f;
    }

private:
    float sampleRate_ = 48000.0f;
    float tWow_ = 0.0f;      ///< Time accumulator for wow modulation
    float tFlutter_ = 0.0f;  ///< Time accumulator for flutter modulation

    // Permutation table for Perlin noise (512 entries: p[256] duplicated)
    uint8_t perm_[512];

    // Original permutation table from Ken Perlin
    static constexpr uint8_t kPerlinP[256] = {
        151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225,
        140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120,
        234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177,
        33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165,
        71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211,
        133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25,
        63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
        135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217,
        226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206,
        59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248,
        152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22,
        39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246,
        97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51,
        145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84,
        204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114,
        67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180};

    /**
     * Perlin fade curve: 6t^5 - 15t^4 + 10t^3
     * Provides smooth interpolation with zero first and second derivatives at t=0 and t=1
     */
    inline float fade(float t) const
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    /**
     * Linear interpolation
     */
    inline float lerp(float t, float a, float b) const
    {
        return a + t * (b - a);
    }

    /**
     * Simple 1D gradient function for Perlin noise
     * Maps hash to either +x or -x
     */
    inline float grad(int hash, float x) const
    {
        return (hash & 1) == 0 ? x : -x;
    }

    /**
     * Generate 1D Perlin noise value
     * @param x Input coordinate for noise lookup
     * @return Noise value in approximate range [-1, 1]
     */
    float Perlin1D(float x) const
    {
        // Find lattice coordinate
        int X = static_cast<int>(floorf(x)) & 255;
        
        // Relative position within cell
        x -= floorf(x);
        
        // Compute fade curve value
        float u = fade(x);

        // Hash coordinates
        int a = perm_[X];
        int b = perm_[X + 1];

        // Interpolate between gradients
        return lerp(u, grad(a, x), grad(b, x - 1.0f));
    }

    /**
     * Generate Fractal Brownian Motion using layered Perlin noise
     * @param x Input coordinate for noise lookup
     * @param octaves Number of noise layers to combine (typically 1-4)
     * @param lacunarity Frequency multiplier between octaves (typically 2.0)
     * @param gain Amplitude multiplier between octaves (typically 0.5)
     * @return Normalized FBM value in range approximately [-1, 1]
     */
    float Fbm1D(float x, int octaves, float lacunarity, float gain) const
    {
        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxSum = 0.0f;

        for (int i = 0; i < octaves; i++)
        {
            sum += Perlin1D(x * frequency) * amplitude;
            maxSum += amplitude;
            frequency *= lacunarity;
            amplitude *= gain;
        }

        // Normalize to approximate range [-1, 1]
        return sum / maxSum;
    }
};
