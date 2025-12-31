# Migration Guide: Using Core DSP Library in Firmware

This document explains how to update the firmware to use the shared `core/` DSP library.

## Changes Required

### 1. Update Include Paths

In `firmware/Makefile`, add the core directory to the include path:

```makefile
C_INCLUDES += -I$(PROJECT_ROOT)/core
```

Or if using the parent directory convention:
```makefile
C_INCLUDES += -I$(PROJECT_ROOT)/..
```

### 2. Update Source Files

#### Option A: Keep firmware files as wrappers (recommended for gradual migration)

Update firmware headers to include from core:

```cpp
// firmware/src/effects/base_effect.h
#pragma once
#include "core/effects/base_effect.h"
```

```cpp
// firmware/src/audio/tempo.h
#pragma once
#include "core/audio/tempo.h"
```

#### Option B: Update includes directly

Change all includes in firmware from:
```cpp
#include "effects/base_effect.h"
```
to:
```cpp
#include "core/effects/base_effect.h"
```

### 3. Update audio_engine.h

The firmware's `AudioEngine` can use `AudioProcessor` from core or continue with its own implementation. The key difference is buffer management:

**Firmware approach (SDRAM):**
- Buffers allocated statically with `DSY_SDRAM_BSS`
- `BindDelayBuffers()`, `BindSweepBuffers()`, `BindReverbBuffers()` called in constructor

**Core approach (platform-agnostic):**
- Buffers provided externally via `BindDelayBuffers()` etc.
- Platform decides how to allocate (SDRAM, heap, etc.)

### 4. Files to Remove from Firmware

Once migrated, these become redundant in firmware:
- `firmware/src/effects/*.h` (use `core/effects/*.h`)
- `firmware/src/effects/effect_registry.cpp` (use `core/effects/effect_registry.cpp`)
- `firmware/src/patch/patch_protocol.h` (use `core/patch/patch_protocol.h`)
- `firmware/src/audio/tempo.h` (use `core/audio/tempo.h`)
- `firmware/src/audio/pedalboard.h` (use `core/audio/pedalboard.h`)

### 5. Files to Keep in Firmware

Platform-specific files that should remain:
- `firmware/src/main.cpp` - DaisySeed hardware init
- `firmware/src/effects/static_defs.cpp` - SDRAM buffer allocation
- `firmware/src/midi/midi_control.cpp/.h` - USB MIDI transport
- `firmware/src/audio/audio_engine.cpp/.h` - if keeping custom engine
- `firmware/src/patch/ui_control.cpp/.h` - hardware buttons/UI
- `firmware/src/audio/tempo_control.cpp/.h` - hardware tempo features

### 6. Chorus Effect Note

The original firmware chorus uses DaisySP's `Chorus` class. The core library provides a pure C++ implementation that doesn't require DaisySP. If you want to use DaisySP in firmware:

```cpp
// In firmware, you can override with DaisySP version:
#define USE_DAISYSP_CHORUS 1

#if USE_DAISYSP_CHORUS
#include "daisysp.h"
// ... use DaisySP implementation
#else
#include "core/effects/chorus.h"
// ... use pure C++ implementation
#endif
```

## Build System Integration

### CMake (for core library)

The core library can be built with CMake:

```cmake
add_library(core_dsp STATIC
    core/audio/audio_processor.cpp
    core/effects/effect_registry.cpp
    core/effects/effect_static_defs.cpp
)
target_include_directories(core_dsp PUBLIC ${CMAKE_SOURCE_DIR})
```

### Makefile (for firmware)

Add core sources to firmware build:

```makefile
C_SOURCES += \
    ../core/audio/audio_processor.cpp \
    ../core/effects/effect_registry.cpp \
    ../core/effects/effect_static_defs.cpp
```

Or compile as a separate library and link.

## Testing Strategy

1. **Unit tests** - Test core library independently (see `vst/src/standalone_test.cpp`)
2. **VST testing** - Test effect chain in DAW
3. **Hardware testing** - Test on Daisy Seed with real audio

The core library enables all three without code duplication.
