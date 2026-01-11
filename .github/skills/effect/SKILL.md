---
name: daisy-seed-effect
description: Contains tools and documentation for how effects are created and managed within the DaisySeed Multi-Effect pedal project.
---

# Effect Development Guide

This document covers everything needed to create and modify audio effects for the DaisySeed Multi-Effect pedal.

## Architecture Overview

Effects are defined in `core/effects/` and are **cross-platform** - the same code runs on both:

- **Firmware** (Daisy Seed hardware)
- **VST/AU plugin** (desktop)

### Key Files

| File                             | Purpose                                   |
| -------------------------------- | ----------------------------------------- |
| `core/effects/base_effect.h`     | Base class and interfaces                 |
| `core/effects/effect_metadata.h` | Metadata registry (names, params, ranges) |
| `core/effects/<effect>.h`        | Individual effect implementations         |
| `core/audio/audio_processor.h`   | Effect instantiation and routing          |

## Signal Levels and Gain Staging

### ⚠️ CRITICAL: Input Signal Levels

The `AudioProcessor` applies **input gain staging** before effects receive audio:

```cpp
// In audio_processor.h
float inputGain_ = 8.0f;  // ~+18dB boost
float outputGain_ = 1.0f; // Unity output
```

**Why this matters:**

- Guitar pickups output **instrument level** (~0.05 to 0.15 peak)
- Effects expect **line level** (~0.5 to 1.0 peak)
- The 8x input gain brings 0.1 → 0.8

**When designing effects:**

- Assume input signals are in the **-1.0 to +1.0** range
- Typical guitar signal after gain staging: **±0.4 to ±0.8**
- Design clipping/saturation thresholds accordingly

### Output Level Guidelines

Effects should maintain **unity gain** or close to it:

- Input level ≈ Output level (when effect is subtle)
- Avoid boosting output significantly - let the user control volume elsewhere
- For effects that compress/clip, compensate to maintain perceived loudness

### Common Gain Staging Mistakes

1. **pre_gain at zero for bypass**: If your effect has a "drive" or "amount" parameter that scales the input, ensure it's **at least 1.0** at minimum setting:

   ```cpp
   // BAD: At drive=0, no signal passes through!
   pre_gain_ = drive * 10.0f;

   // GOOD: Unity gain at minimum, boost above
   pre_gain_ = 1.0f + drive * 9.0f;
   ```

2. **Over-compensating auto-leveling**: Don't boost quiet signals too much:

   ```cpp
   // BAD: Can boost output 4x at low settings
   post_gain_ = 1.0f / processed_level;

   // GOOD: Gentle compensation with limits
   post_gain_ = 1.0f / (1.0f + 0.3f * drive);
   ```

## Creating a New Effect

### Step 1: Define Metadata

Add to `core/effects/effect_metadata.h`:

```cpp
namespace Effects
{
    namespace MyEffect
    {
        constexpr uint8_t TypeId = 99; // Unique ID

        inline const NumberParamRange kAmountRange = {0.0f, 1.0f, 0.01f};

        inline const ParamInfo kParams[] = {
            {0, "Amount", "Effect intensity", ParamValueKind::Number, &kAmountRange, nullptr},
            {1, "Tone", "Brightness control", ParamValueKind::Number, nullptr, nullptr},
        };

        inline const ::EffectMeta kMeta = {
            "My Effect",     // Full name
            "MFX",           // 3-char short name
            "Description.",  // Help text
            kParams,
            2                // Number of parameters
        };
    }
}
```

Don't forget to add to `kAllEffects[]` array at the bottom of the file.

### Step 2: Implement the Effect

Create `core/effects/my_effect.h`:

```cpp
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include <cmath>

struct MyEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::MyEffect::TypeId;

    // Parameters (store as 0-1 normalized)
    float amount_ = 0.5f;
    float tone_ = 0.5f;

    // Internal state
    float sampleRate_ = 48000.0f;

    // Required overrides
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }
    const EffectMeta &GetMetadata() const override { return Effects::MyEffect::kMeta; }

    void Init(float sr) override
    {
        sampleRate_ = sr;
        // Initialize DSP state
    }

    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: amount_ = v; break;
        case 1: tone_ = v; break;
        }
    }

    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 2) return 0;
        out[0] = {0, (uint8_t)(amount_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(tone_ * 127.0f + 0.5f)};
        return 2;
    }

    void ProcessStereo(float &l, float &r) override
    {
        // Your DSP code here
        // Input: l, r in range [-1, 1] (after input gain staging)
        // Output: modify l, r in place
    }
};
```

### Step 3: Register in AudioProcessor

In `core/audio/audio_processor.h`:

1. Include the header
2. Add to effect pool arrays
3. Add pool counter
4. Add to `Instantiate()` switch

## Effect Design Patterns

### Soft Clipping (Overdrive/Distortion)

```cpp
// Soft limiting from Mutable Instruments
static inline float SoftLimit(float x)
{
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// Soft clip with hard limits at ±1
static inline float SoftClip(float x)
{
    if (x < -3.0f) return -1.0f;
    else if (x > 3.0f) return 1.0f;
    else return SoftLimit(x);
}
```

### Envelope Follower (Compressor/Gate)

```cpp
float attackCoef = std::exp(-1.0f / (attackTime * sampleRate_));
float releaseCoef = std::exp(-1.0f / (releaseTime * sampleRate_));

float inputLevel = std::fabs(sample);
if (inputLevel > envelope_)
    envelope_ = attackCoef * envelope_ + (1.0f - attackCoef) * inputLevel;
else
    envelope_ = releaseCoef * envelope_;
```

### Parameter Mapping

Parameters come in as 0-1 normalized. Map to useful ranges:

```cpp
// Linear: 0..1 → 10ms..1000ms
float timeMs = 10.0f + v * 990.0f;

// Exponential (for frequency/time): 0..1 → 20Hz..20kHz
float freq = 20.0f * std::pow(1000.0f, v);

// dB scale: 0..1 → -40dB..0dB → linear
float dB = -40.0f + v * 40.0f;
float linear = std::pow(10.0f, dB / 20.0f);
```

### One-Pole Lowpass Filter (Tone Control)

```cpp
// coeff closer to 0 = more filtering (darker)
// coeff closer to 1 = less filtering (brighter)
float coeff = 0.05f + 0.4f * tone_;  // tone_ is 0-1
lpState_ += coeff * (input - lpState_);
output = lpState_;
```

## Channel Modes

```cpp
enum class ChannelMode : uint8_t
{
    Mono,         // Effect only processes mono
    Stereo,       // Effect requires stereo
    MonoOrStereo  // Effect works with either
};
```

The pedalboard handles routing based on this:

- `sumToMono` flag can mix L+R before effect
- `ChannelPolicy` controls output behavior

## Testing Effects

### In VST (Recommended for Development)

1. Build: `cd vst && cmake -B build && cmake --build build`
2. Run standalone: `./build/DaisyMultiFX_artefacts/Release/Standalone/DaisyMultiFX.app/Contents/MacOS/DaisyMultiFX`
3. Or load in DAW as AU/VST3

### On Hardware

1. Build: `cd firmware && make`
2. Flash: `make flash` (with Daisy in DFU mode)

## Debugging Tips

1. **No sound?** Check that `pre_gain` isn't zero at low parameter values
2. **Too loud?** Check `post_gain` compensation isn't over-boosting
3. **Clipping at output?** Reduce effect output or `outputGain_` in AudioProcessor
4. **Effect not responding to drive?** Signal might be too low - check input levels

## Reference: Effect Type IDs

| ID  | Effect             |
| --- | ------------------ |
| 0   | Off (bypass)       |
| 1   | Delay              |
| 10  | Overdrive          |
| 11  | Reverb             |
| 12  | Stereo Sweep Delay |
| 13  | Stereo Mixer       |
| 14  | Compressor         |
| 15  | Chorus             |
| 16  | Noise Gate         |
| 17  | Graphic EQ         |
| 18  | Flanger            |
| 19  | Phaser             |
| 20  | Neural Amp         |
| 21  | Cabinet IR         |

See `effect_metadata.h` for the complete, authoritative list.
