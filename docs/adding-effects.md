# Adding a New Effect

This guide explains how to add a new audio effect to the DaisyMultiFX project.

## Overview

Effects are defined once in `core/effects/` and shared between the firmware (Daisy Seed) and the VST plugin. This ensures identical DSP behavior on both platforms.

## Steps

### 1. Create the Effect Header

Create a new file in `core/effects/` (e.g., `my_effect.h`):

```cpp
#pragma once
#include "effects/base_effect.h"
#include <cmath>
#include <algorithm>

struct MyEffect : BaseEffect
{
    static constexpr uint8_t TypeId = 18;  // Choose next available ID

    // Parameters (normalized 0..1 internally)
    float param1_ = 0.5f;
    float param2_ = 0.5f;

    // Internal state
    float sampleRate_ = 48000.0f;

    // Metadata (defined inline at bottom of file)
    static const ParamInfo kParams[];
    static const EffectMeta kMeta;
    const EffectMeta &GetMetadata() const override { return kMeta; }

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

    void ProcessStereo(float &l, float &r) override
    {
        // Your DSP code here
    }
};

// Static metadata
inline const ParamInfo MyEffect::kParams[] = {
    {0, "Param1", "Description", ParamValueKind::Number, nullptr, nullptr},
    {1, "Param2", "Description", ParamValueKind::Number, nullptr, nullptr},
};

inline const EffectMeta MyEffect::kMeta = {
    "My Effect",
    "Short description of the effect.",
    MyEffect::kParams,
    2  // numParams
};
```

### 2. Add to Effect Metadata

Edit `core/effects/effect_metadata.h`:

1. Add a new namespace with metadata:
```cpp
namespace MyEffect
{
    constexpr uint8_t TypeId = 18;
    inline const ParamInfo kParams[] = { /* ... */ };
    inline const ::EffectMeta kMeta = {"My Effect", "Description", kParams, 2};
}
```

2. Add to the `kAllEffects` array:
```cpp
inline const EffectEntry kAllEffects[] = {
    // ... existing effects ...
    {MyEffect::TypeId, &MyEffect::kMeta},
};
```

### 3. Update Audio Processors

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

### 4. Update Firmware (same changes)

Apply identical changes to:
- `firmware/src/audio/audio_engine.h`
- `firmware/src/audio/audio_engine.cpp`
- `firmware/src/effects/effect_registry.cpp` (add to Lookup switch)

### 5. Build and Test

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
| 10-19 | Dynamics/Distortion |
| 12-13 | Modulation delays |
| 14-16 | Reverb, Compressor, Chorus |
| 17+ | New effects |

## Tips

- Use `std::fabs()` instead of `abs()` for floats
- Include `<algorithm>` for `std::max/min`
- Parameters are 0-1 normalized; convert in `SetParam()`
- Keep DSP simple - this runs at 48kHz per sample
- Test in VST first (easier debugging than firmware)
