# Tape Flutter Utility

The `tape_flutter.h` utility simulates analog tape wow and flutter effects using 1D Perlin noise with Fractal Brownian Motion (FBM). It creates smooth, natural-sounding speed variations that mimic the imperfections of analog tape machines, perfect for adding character to delay effects.

## Reference

Based on the implementation from: https://github.com/bkshepherd/DaisySeedProjects/commit/f2031eb86b56bc69d8f50d3e26b4a90e9bffbf61

## Features

- **Header-only**: No separate compilation needed, just include the header
- **Minimal dependencies**: Uses only FastMath utilities and standard C++ (cstdint)
- **Efficient**: Suitable for real-time audio processing on embedded hardware
- **Perlin noise based**: Uses Ken Perlin's permutation table for natural randomness
- **Two-component modulation**: Separate wow (slow) and flutter (fast) components

## How It Works

### Wow vs Flutter

- **Wow**: Slow speed variations (typically 0.2-2 Hz) - simulates mechanical imperfections in tape transport
- **Flutter**: Fast speed variations (typically 2-7 Hz) - simulates higher-frequency instabilities

Both are generated using Fractal Brownian Motion (FBM), which layers multiple octaves of Perlin noise for smooth, organic variation.

### Parameters

- `wowRate` (Hz): Controls the speed of the slow wow modulation (typically 0.2-2 Hz)
- `flutterRate` (Hz): Controls the speed of the fast flutter modulation (typically 2-7 Hz)
- `wowDepth`: Scales the amount of wow effect (typically 0.5-2.0)
- `flutterDepth`: Scales the amount of flutter effect (typically 0.2-1.0, usually less than wow)

The flutter component is automatically scaled down by 0.2x relative to its depth parameter to maintain realistic proportions.

## Usage Example

### Basic Integration in a Delay Effect

```cpp
#include "core/effects/tape_flutter.h"
#include "core/effects/base_effect.h"

struct TapeDelayEffect : BaseEffect {
    TapeFlutter tapeFlutter_;
    float sampleRate_;
    float baseDelayMs_ = 250.0f;  // 250ms base delay
    
    // ... other delay state (buffers, write position, etc.)
    
    void Init(float sr) override {
        sampleRate_ = sr;
        tapeFlutter_.Init(sr);
        // ... initialize other delay components
    }
    
    void ProcessStereo(float &l, float &r) override {
        // Get tape speed modulation
        float speedMod = tapeFlutter_.GetTapeSpeed(
            0.5f,  // wow_rate: 0.5 Hz slow modulation
            3.0f,  // flutter_rate: 3 Hz fast modulation
            1.0f,  // wow_depth: moderate amount
            0.5f   // flutter_depth: subtle flutter
        );
        
        // Apply modulation to delay time
        // speedMod is roughly in range [-1, 1]
        float modulationAmount = 50.0f;  // ±50 samples modulation
        float baseDelaySamples = baseDelayMs_ * 0.001f * sampleRate_;
        float modulatedDelay = baseDelaySamples + speedMod * modulationAmount;
        
        // Clamp to valid range
        if (modulatedDelay < 1.0f) 
            modulatedDelay = 1.0f;
        if (modulatedDelay > maxDelay)
            modulatedDelay = maxDelay;
        
        // ... use modulatedDelay for delay buffer read position
        // ... rest of delay processing
    }
};
```

### Parameter Control

You can expose tape flutter parameters to users:

```cpp
void SetParam(uint8_t id, float v) override {
    switch (id) {
        case 0: // Wow rate (map 0-1 to 0.2-2 Hz)
            wowRate_ = 0.2f + v * 1.8f;
            break;
        case 1: // Flutter rate (map 0-1 to 2-7 Hz)
            flutterRate_ = 2.0f + v * 5.0f;
            break;
        case 2: // Tape character (controls both wow and flutter depth)
            wowDepth_ = v * 2.0f;      // 0-2.0
            flutterDepth_ = v * 1.0f;  // 0-1.0
            break;
        // ... other parameters
    }
}
```

### Dynamic Min Delay (Preventing Negative Delays)

When using strong modulation, ensure the delay time never goes negative:

```cpp
// Calculate minimum safe delay time based on modulation amount
float maxModulation = wowDepth_ + 0.2f * flutterDepth_;  // Max amplitude
float minSafeDelay = 1.0f + maxModulation * modulationAmount;

// Use this as the minimum for your delay range
float delayRange = maxDelay - minSafeDelay;
float baseDelay = minSafeDelay + delayParam * delayRange;

// Now apply modulation
float modulatedDelay = baseDelay + speedMod * modulationAmount;
```

## Output Characteristics

The `GetTapeSpeed()` method returns a float value with these characteristics:

- **Range**: Approximately -0.5 to +0.5 with default parameters (not strictly bounded)
- **Mean**: Near 0 over time
- **Distribution**: Smooth, continuous variations (not random noise)
- **Temporal coherence**: Strongly correlated between adjacent samples

Typical output statistics (measured over 10 seconds at 48kHz with default parameters: wow_rate=0.5Hz, flutter_rate=3.0Hz, wow_depth=1.0, flutter_depth=0.5):
- Min: ~-0.41
- Max: ~+0.37
- Range: ~0.78
- Mean: ~0 (close to zero over time)

## Performance Considerations

- **CPU Usage**: Very light - just a few floating-point operations per sample
- **Memory**: ~520 bytes (512-byte permutation table + state variables)
- **No dynamic allocation**: All memory is pre-allocated
- **Real-time safe**: No system calls, no branching in hot path (except wrap checks)

## Tips for Best Results

1. **Start subtle**: Begin with low depth values (0.5-1.0) and adjust to taste
2. **Match your delay time**: Longer delays can handle more modulation
3. **Consider tempo**: For tempo-synced delays, you may want to sync modulation rates too
4. **Stereo variation**: Use two instances with slightly different parameters for wider stereo image
5. **Combine with other modulation**: Can be mixed with LFO-based modulation for complex effects

## Technical Details

### Perlin Noise
Uses Ken Perlin's improved noise algorithm with:
- 256-entry permutation table (duplicated to 512 for efficient wraparound)
- Fade curve: 6t^5 - 15t^4 + 10t^3 (smooth interpolation)
- 1D gradient function (simple ±x mapping)

### Fractal Brownian Motion
- Wow uses 2 octaves (more smooth, organic)
- Flutter uses 1 octave (simpler, faster variations)
- Lacunarity: 2.0 (doubles frequency each octave)
- Gain: 0.5 (halves amplitude each octave)

### Time Accumulator Management
Time accumulators wrap at 256.0 to prevent floating-point precision loss while maintaining seamless continuity (Perlin noise uses modulo 256 for lattice coordinates).
