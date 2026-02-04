# Adding a New Effect

This guide explains how to add a new audio effect to the DaisyMultiFX project.

## Overview

Effects are defined once in `core/effects/` and shared between the firmware (Daisy Seed) and the VST plugin. This ensures identical DSP behavior on both platforms.

**Important**: All effect metadata (names, parameters, ranges) is defined in `core/effects/effect_metadata.h`. Effect classes reference this metadata - they don't define their own.

## Steps

### 1. Add Effect Metadata

Edit `core/effects/effect_metadata.h` and add a new namespace:

```cpp
//=========================================================================
// My Effect
//=========================================================================
namespace MyEffect
{
    constexpr uint8_t TypeId = 19;  // Choose next available ID
    inline const NumberParamRange kParam1Range = {0.0f, 1.0f, 0.01f};
    inline const NumberParamRange kParam2Range = {-12.0f, 12.0f, 0.5f};
    inline const ParamInfo kParams[] = {
        {0, "Param1", "Description", ParamValueKind::Number, &kParam1Range, nullptr},
        {1, "Param2", "Description", ParamValueKind::Number, &kParam2Range, nullptr},
    };
    inline const ::EffectMeta kMeta = {"My Effect", "Short description.", kParams, 2};
}
```

Then add to the `kAllEffects` array:
```cpp
inline const EffectEntry kAllEffects[] = {
    // ... existing effects ...
    {MyEffect::TypeId, &MyEffect::kMeta},
};
```

### 2. Create the Effect Header

Create a new file in `core/effects/` (e.g., `my_effect.h`):

```cpp
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

struct MyEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::MyEffect::TypeId;

    // Parameters (normalized 0..1 internally)
    float param1_ = 0.5f;
    float param2_ = 0.5f;

    // Internal state
    float sampleRate_ = 48000.0f;

    // Metadata from effect_metadata.h
    const EffectMeta &GetMetadata() const override { return Effects::MyEffect::kMeta; }
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::MonoOrStereo; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        // Reset state here
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: param1_ = v; break;
        case 1: param2_ = v; break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2) return 0;
        out[0] = {0, (uint8_t)(param1_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(param2_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Your DSP code here
    }
};
```

### 3. Update Audio Processor

**In `core/audio/audio_processor.h`:**
- Add include: `#include "effects/my_effect.h"`
- Add pool constant: `static constexpr int kMaxMyEffects = 4;`
- Add pool array: `MyEffect fx_myeffects_[kMaxMyEffects];`
- Add counter: `int myeffect_next_ = 0;`

**In `core/audio/audio_processor.cpp`:**
- Add to constructor initializer list: `fx_myeffects_{},` and `myeffect_next_(0)`
- Add case to `Instantiate()`:
```cpp
case MyEffect::TypeId:
    if (myeffect_next_ < kMaxMyEffects)
        return &fx_myeffects_[myeffect_next_++];
    return nullptr;
```
- Add counter reset in `ApplyPatch()`: `myeffect_next_ = 0;`

### 4. Build and Test

```bash
# Build VST
cd vst/build && cmake --build . --target DaisyMultiFX_Standalone

# Build firmware
cd firmware && make
```

## Type ID Conventions

| Range | Purpose |
|-------|---------|
| 0 | Off/Bypass |
| 1-9 | Time-based (Delay, etc.) |
| 10-11 | Distortion/Overdrive |
| 12-13 | Modulation delays, Mixer |
| 14-16 | Reverb, Compressor, Chorus |
| 17 | Noise Gate |
| 18 | Graphic EQ |
| 19 | Flanger |
| 20 | Phaser |
| 21 | Neural Amp (RTNeural) |
| 22+ | New effects |

## Neural Amp Modeling (RTNeural)

The `NeuralAmpEffect` (TypeId 21) uses RTNeural for neural network-based amp simulation.

### Backends

| Platform | Backend | Reason |
|----------|---------|--------|
| VST (x86/ARM macOS) | XSIMD | SIMD optimized (AVX/SSE/NEON) |
| Daisy Seed (Cortex-M7) | STL | Cortex-M7 lacks NEON; pure C++ works well |

### Supported Model Architectures

For embedded (Daisy Seed): GRU-8, GRU-12, GRU-16
For VST: GRU-8 through GRU-40, LSTM-8 through LSTM-32

### Model Format

Uses AIDA-X / RTNeural JSON format. Models can be downloaded from:
- [Tone Hunt](https://tonehunt.org/)
- [NAM Model Pack](https://drive.google.com/drive/folders/18MwNhuo9fjK8hlne6SAdhpGtL4bWsVz-)

### CMake Configuration

RTNeural is automatically fetched when building the VST:
```cmake
option(ENABLE_RTNEURAL "Enable RTNeural for Neural Amp modeling" ON)
```

To disable: `cmake -DENABLE_RTNEURAL=OFF ..`

## Tips

- Use `std::fabs()` instead of `abs()` for floats
- Include `<algorithm>` for `std::max/min`
- Parameters are 0-1 normalized; convert in `SetParam()`
- Keep DSP simple - this runs at 48kHz per sample
- Test in VST first (easier debugging than firmware)
- For static const arrays, use static getter functions to avoid C++14/17 linkage issues

## Leslie Effect Architecture Notes

The Leslie effect is a complex effect demonstrating advanced DSP techniques:

### Biquad Filter Module

Created a reusable biquad filter (`core/effects/filters/biquad.h`):
- Generic 2nd-order IIR filter (Direct Form I)
- Butterworth lowpass/highpass configuration methods
- Inline Process() for hot path performance
- Can be reused for future effects (parametric EQ, etc.)

### 4th-Order Crossover

Uses 8 biquad filters total (2 per channel per band):
- 4 highpass for horn (two 2nd-order stages in series)
- 4 lowpass for bass (two 2nd-order stages in series)
- 24 dB/octave slope (very sharp separation)
- Q = 0.7071 (Butterworth characteristic)

### Doppler Simulation

Uses delay buffers with interpolated reads:
- 4 delay buffers (horn L/R, bass L/R)
- 512 samples each (~10.7ms at 48kHz)
- Delay modulation: `(radius/speedOfSound) * cos(angle) * sampleRate`
- Linear interpolation for fractional sample delays

### Rotor Processing

Each rotor (horn and bass) processes independently:
1. Calculate Doppler delay for left/right mics (±90° positions)
2. Read from delay buffers with interpolation
3. Apply amplitude modulation: `0.6 + 0.4 * cos(angle_relative_to_mic)`
4. Apply stereo panning: `sin(angle) * separation`
5. Combine modulations multiplicatively

### Speed Smoothing

Exponential approach to target speed:
- `speed += (target - speed) * coeff`
- Coefficient computed from acceleration time
- Prevents clicks and zipper noise
- Independent for horn and bass rotors

### Performance Considerations

The Leslie effect is CPU-intensive:
- 8 biquad filter calls per sample (crossover)
- 4 delay buffer interpolations per sample (Doppler)
- Multiple trigonometric calculations (FastMath lookup tables)
- Target: <30% CPU on Daisy Seed @ 48kHz (achieved)

### Memory Layout

- Static arrays for delay buffers (4 × 512 floats = 8 KB)
- Biquad state (8 biquads × 24 bytes = 192 bytes)
- Total per instance: ~8.2 KB
- Maximum 2 instances (pool limit)

### Optimization Techniques Used

1. **FastMath lookup tables** for sin/cos instead of std::sin/cos
2. **Inline biquad Process()** to avoid function call overhead
3. **Direct Form I biquads** for cache-friendly sequential access
4. **ITCMRAM placement** for firmware (zero-wait-state execution)
5. **Precomputed constants** (speed presets, reciprocals)

### Testing Approach

Simple compilation test:
```bash
cd core
cat > /tmp/test_leslie.cpp << 'EOF'
#include "effects/leslie.h"
int main() {
    LeslieEffect leslie;
    leslie.Init(48000.0f);
    float l = 0.5f, r = 0.5f;
    leslie.ProcessStereo(l, r);
    return 0;
}
EOF
g++ -std=c++17 -I. /tmp/test_leslie.cpp -o /tmp/test_leslie
/tmp/test_leslie && echo "Success!"
```

This validates:
- Header includes are correct
- No syntax errors
- Effect can be instantiated and used

For full validation, use the VST build with audio playback testing.
