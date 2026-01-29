# Daisy Seed Multi-Effect Pedal - Project Skills

## Project Overview
A modular multi-effect guitar pedal platform using Daisy Seed (STM32H7 Cortex-M7 @ 480MHz). Includes a core DSP library (C++17, header-only), Daisy Seed firmware, VST3 plugin, iOS controller app, and MIDI bridge.

## Architecture
- **12-slot pedalboard** with arbitrary routing (serial, parallel via mixer)
- **Sample-by-sample** processing at 48kHz, 48-sample block size (1ms latency)
- **Platform-agnostic core** in `core/` shared by firmware and VST
- **MIDI SysEx protocol** for real-time control (manufacturer ID 0x7D)

## Key Directories
- `core/effects/` - All effect implementations (header-only)
- `core/audio/` - Audio processor, pedalboard runtime
- `core/protocol/` - MIDI SysEx encoder/decoder
- `core/effects/embedded/` - Baked-in model weights and IR data
- `firmware/src/` - Daisy Seed entry point, MIDI control, effect registry
- `vst/src/` - VST3 plugin (JUCE-based)
- `lib/` - RTNeural, libDaisy, DaisySP submodules

## Build
- Firmware: `cd firmware && make -j && make flash` (requires DFU bootloader)
- Debug patches: `make -j DEBUG_PATCH=1` (passthrough) or `make -j DEBUG_NEURALAMP=1`
- VST: Built via CMake/JUCE
- Compiler flags: `-O3 -ffast-math -funroll-loops -flto`

## Effect System
- All effects extend `BaseEffect` (see `core/effects/base_effect.h`)
- Effect metadata centralized in `core/effects/effect_metadata.h`
- Parameters are normalized 0-1 floats, communicated as 7-bit MIDI values
- Effect pools with fixed instance counts in `audio_processor.h`
- Max 2 Neural Amps and 2 Cabinet IRs per patch (CPU budget)

## Hardware Constraints (Daisy Seed)
- **CPU**: Cortex-M7 @ 480MHz, single-precision FPU, NO NEON/SIMD
- **SRAM**: 256KB internal + 64MB external SDRAM
- **Flash**: 8MB QSPI (boot from QSPI via bootloader)
- **Audio**: 48kHz, 24-bit codec, 48-sample blocks
- **Budget**: ~10,000 cycles/sample for entire effects chain

---

# Neural Amp Modeling - Skills File

## Current Implementation
- **File**: `core/effects/neural_amp.h`
- **Architecture**: GRU-9 with residual connection (RTNeural library, Mars approach)
- **Models**: 8 embedded GRU-9 models (converted from Mars project to constexpr arrays)
  - Fender 57, Matchless SC30, Klon BB, Mesa IIC, HAK Clean, Bassman, 5150, Splawn
- **Model weights**: `core/effects/embedded/models/gru9_models.h`
- **Model registry**: `core/effects/embedded/model_registry.h`
- **Backend**: STL on firmware (no SIMD), XSIMD on VST
- **Residual connection**: `output = model.forward(input) + input` (improves quality for small models)
- **Level adjust**: Per-model output level compensation stored in model registry

## Reference Projects Analyzed

### Mercury (WaveNet "Pico")
- **Location**: `/tmp/mercury/`
- **NN Type**: Custom WaveNet with dilated causal convolutions
- **Architecture**: 2 layer arrays
  - Layer 1: 7 dilations [1,2,4,8,16,32,64], 2 channels, kernel=3
  - Layer 2: 13 dilations [128,256,512,1,2,4,8,16,32,64,128,256,512], 2 channels, kernel=3
- **Params per model**: ~1,700 weights (~6.8KB)
- **MACs per sample**: ~2,000
- **Total (9 models)**: ~88KB
- **Key optimizations**:
  - Custom fast tanh approximation (polynomial, ~3rd order)
  - Memory arena (no dynamic allocation in hot path)
  - Eigen backend for matrix ops
  - `-Ofast` compiler flag
  - `RTNEURAL_USE_EIGEN=1`
- **Boot mode**: BOOT_SRAM (256KB limit, dedicated amp sim only)
- **Tone shaping**: 4-band peaking EQ post-model
- **Sound quality**: Higher (WaveNet captures temporal/convolutional characteristics better)

### Mars (GRU-9)
- **Location**: `/tmp/mars/`
- **NN Type**: GRU (Gated Recurrent Unit), hidden size 9
- **Architecture**: GRU(1→9) → Dense(9→1)
- **Params per model**: ~340 weights (~1.4KB)
- **MACs per sample**: ~350-400
- **Key comment from source**: "GRU 10 is max for single-knob, GRU 8 with effects is max, GRU 9 recommended for multi-effect"
- **Key optimizations**:
  - `-Os` (optimized for size, NOT speed)
  - Residual connection: `model.forward(input) + input` (helps low-parameter models)
  - Boot mode: BOOT_SRAM
- **Additional effects**: Delay (2-tap), tone filter, IR convolution
- **Sound quality**: Lower (fewer parameters, less expressive)

## Performance Comparison (per sample at 48kHz)

| Metric | Mars (GRU-9) | Mercury (WaveNet Pico) | Current (LSTM-12) |
|--------|-------------|----------------------|-------------------|
| MACs/sample | ~400 | ~2,000 | ~700 |
| Weights/model | ~340 | ~1,700 | ~685 |
| Memory (9 models) | ~12KB | ~88KB | ~24KB |
| Quality | Low-Med | High | Medium |
| Multi-FX headroom | Best | Worst | Good |
| Complexity | Simple | Complex (custom impl) | Simple |

## Recommendation for Multi-Effect Pedal

**GRU-9 (Mars approach) is the best fit** for the multi-effect context because:

1. **CPU headroom**: ~400 MACs/sample vs ~2,000 (WaveNet) or ~700 (LSTM-12), leaving budget for 11 other effects
2. **Battle-tested**: Mars author explicitly recommends GRU-9 for multi-effect setups
3. **Residual connection trick**: `output = model.forward(input) + input` significantly improves quality for small models
4. **Memory**: Tiny footprint (~12KB for 9 models) - critical when sharing SRAM with other effects
5. **Simplicity**: Standard RTNeural API, no custom WaveNet implementation needed

**GRU-12 is a reasonable upgrade** if quality needs improvement:
- ~550 MACs/sample (still much less than WaveNet)
- ~24KB for 9 models
- Better expressiveness than GRU-9

**WaveNet Pico (Mercury) is NOT recommended** because:
- 5x more CPU than GRU-9 (~2,000 MACs)
- Requires custom WaveNet implementation (not standard RTNeural)
- 88KB for 9 models (significant SRAM pressure)
- Designed for dedicated amp sim, not multi-effect
- Uses BOOT_SRAM (256KB limit) - multi-effect needs BOOT_QSPI with more code space

## Implementation Notes

### GRU-9 Weight Layout
GRU has 3 gates (reset, update, new) vs LSTM's 4:
- `weightIH`: input-to-hidden [1 × 3*hidden] = [1 × 27]
- `weightHH`: hidden-to-hidden [hidden × 3*hidden] = [9 × 27]
- `bias`: [2 × 3*hidden] = [2 × 27] (two bias vectors)
- `denseW`: [1 × hidden] = [1 × 9]
- `denseB`: [1]

### Adding New Models
1. Train GRU-9 model at 48kHz using GuitarML Colab notebook
2. Convert weights to constexpr arrays in `core/effects/embedded/models/gru9_models.h`
3. Add entry to `model_registry.h` registry array with levelAdjust value
4. Update enum in `model_registry.h` and model options in `effect_metadata.h`
5. Copy updated files to `firmware/src/effects/embedded/`

### Key Compiler Flags for Neural Amp
```makefile
OPT = -O3 -ffast-math -funroll-loops
LDFLAGS += -flto
CPPFLAGS += -DRTNEURAL_DEFAULT_ALIGNMENT=8 -DRTNEURAL_NO_DEBUG=1
# For Eigen backend (if using WaveNet): -DRTNEURAL_USE_EIGEN=1
# For STL backend (recommended for GRU/LSTM on Cortex-M7): -DRTNEURAL_USE_STL=1
```

### Model Training
- Train at 48kHz (Daisy Seed native rate)
- Use snapshot models (not conditioned) for simplicity and performance
- Input gain serves as pseudo-drive parameter
- Level adjust per model compensates output differences
- GRU-9 models can be trained with the GuitarML Colab notebook
