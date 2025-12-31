# Core DSP Library

This folder contains **platform-agnostic** audio processing code that can be shared between:

- **Firmware** - Running on Daisy Seed hardware
- **VST Plugin** - For testing in a DAW
- **Host applications** - Desktop apps for development/testing

## Structure

```
core/
├── effects/          # Effect implementations
│   ├── base_effect.h
│   ├── delay.h
│   ├── distortion.h
│   ├── reverb.h
│   ├── chorus.h
│   ├── compressor.h
│   ├── stereo_mixer.h
│   ├── stereo_sweep_delay.h
│   ├── time_synced_effect.h
│   ├── effect_registry.h
│   └── effect_registry.cpp
├── audio/            # Audio processing abstractions
│   ├── audio_processor.h
│   ├── audio_processor.cpp
│   ├── pedalboard.h
│   └── tempo.h
├── patch/            # Patch protocol definitions
│   └── patch_protocol.h
└── midi/             # MIDI protocol utilities (no transport)
    └── sysex_utils.h
```

## Design Principles

1. **No hardware dependencies** - All hardware-specific code (SDRAM, USB, GPIO) stays in the platform folder
2. **Dependency injection** - Effects receive buffer pointers, sample rates, etc. via init/bind methods
3. **Pure C++** - Standard library only (no HAL, no vendor SDKs)
4. **Buffer management abstraction** - Platforms provide their own buffer allocation strategy

## Platform Responsibilities

Each platform (firmware, VST) must:

1. **Allocate buffers** for effects that need them (delays, reverbs)
2. **Bind buffers** to effect instances before use
3. **Provide audio I/O** - read from ADC/plugin input, write to DAC/plugin output
4. **Handle MIDI transport** - USB MIDI, plugin MIDI, etc.
5. **Manage UI** - physical buttons, plugin GUI, etc.
