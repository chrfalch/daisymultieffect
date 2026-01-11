---
name: daisy-seed-pedal
description: Contains documentation and tools for developing on the Daisy Seed embedded audio platform in the DaisySeed Multi-Effect pedal project.
---

# Daisy Seed

The Daisy Seed is an embedded audio platform based on the STM32H750 microcontroller with 64MB of SDRAM. It's designed for audio processing applications like guitar pedals, synthesizers, and multi-effects units. The development environment uses C++ with the libDaisy hardware abstraction library and DaisySP DSP library.

## Project Structure

### Standard Daisy Project Layout

```
project/
├── Makefile                  # Build configuration
├── src/
│   └── main.cpp             # Entry point with Init() and AudioCallback()
├── libDaisy/                # Hardware abstraction layer (submodule)
└── DaisySP/                 # DSP library (submodule)
```

### This Project's Structure

```
firmware/
├── Makefile                 # Defines TARGET, sources, includes
├── src/
│   ├── main.cpp            # Initializes hardware, audio, MIDI
│   ├── effects/            # Effect implementations
│   ├── midi/               # MIDI control handlers
│   ├── patch/              # Patch management
│   └── audio/              # Audio processing logic
lib/
├── libDaisy/               # Hardware abstraction (git submodule)
└── DaisySP/                # DSP primitives (git submodule)
core/                       # Shared core logic (effects, state management)
```

## Development Environment Setup

### Required Toolchain

- **ARM GCC Compiler**: `arm-none-eabi-gcc` for cross-compilation
- **dfu-util**: For flashing firmware via USB
- **openocd**: For debugging with ST-Link
- **make**: Build automation

### Installation (macOS)

```bash
# Install ARM toolchain
brew install --cask gcc-arm-embedded

# Install utilities
brew install dfu-util openocd make
```

### VS Code Extensions

- **Cortex Debug** (marus25): For debugging with ST-Link probes
- **C/C++**: IntelliSense and code navigation

## Building Firmware

### Makefile Configuration

Key variables to define in your Makefile:

```makefile
# Project name (generates TARGET.bin)
TARGET = MyProject

# Library paths (relative or absolute)
LIBDAISY_DIR ?= ../lib/libDaisy
DAISYSP_DIR  ?= ../lib/DaisySP

# Memory configuration
APP_TYPE ?= BOOT_QSPI  # Use external QSPI flash (8MB)
# APP_TYPE = BOOT_NONE  # Use internal flash (128KB, limited)

# Source files
CPP_SOURCES = src/main.cpp \
              src/audio.cpp \
              src/effects.cpp

# Include directories
C_INCLUDES += -Isrc -I../core

# Optimization flags
CFLAGS   += -O2
CXXFLAGS += -O2

# Include Daisy build system
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
```

### Build Commands

```bash
cd firmware

# Clean build directory
make clean

# Build firmware (creates build/TARGET.bin)
make

# Or build with multiple cores
make -j

# Build with custom flags
make DEBUG_PATCH=1
```

### Build Output

- **build/TARGET.bin**: Binary firmware file
- **build/TARGET.elf**: ELF file with debug symbols
- **build/\*.lst**: Assembly listings
- **build/\*.d**: Dependency files

## Flashing Firmware

### Method 1: USB DFU (No Debug Probe Required)

#### Enter Bootloader Mode

1. Hold **BOOT** button
2. Press **RESET** button
3. Release **RESET**, then release **BOOT**

#### Flash via USB

```bash
# List DFU devices
dfu-util -l

# Flash firmware
make flash
# or
make program-dfu
```

**Note**: You may see "Error during download get_status" or "make: \*\*\* [program-dfu] Error 74" - this is normal and can be ignored if the flash succeeded.

### Method 2: ST-Link Debug Probe

#### Hardware Connection

Connect ST-Link V3 mini to Daisy Seed's debug header:

- **Red stripe** faces towards the USB connector
- **Center the connector** on the pins (equal spacing on both sides)
- Use pins: GND, 3V3, SWDIO, SWCLK

#### Flash via Debug Probe

```bash
make program
# Uses openocd internally
```

### VS Code Tasks

Use Command Palette (`Cmd+Shift+P`):

- `task build_all`: Build libraries + project
- `task build_and_program_dfu`: Build and flash via USB
- `task build_and_program`: Build and flash via debug probe

## Debugging

### Enable Debugging

1. Connect ST-Link debug probe to Daisy
2. Open your project folder in VS Code
3. Press **F5** to start debugging
4. Code will halt at default breakpoint
5. Press **Continue** to run

### Debug Configuration

Create `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug Daisy",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "openocd",
      "cwd": "${workspaceRoot}",
      "executable": "${workspaceRoot}/build/TARGET.elf",
      "configFiles": ["interface/stlink.cfg", "target/stm32h7x.cfg"]
    }
  ]
}
```

### Serial Printing for Debugging

```cpp
#include "daisy_seed.h"

daisy::DaisySeed hw;

void Init() {
    hw.Init();
    hw.StartLog();  // Enable USB serial logging
}

void Loop() {
    hw.PrintLine("Debug: value = %d", someValue);
    System::Delay(1000);
}
```

View output: `screen /dev/cu.usbmodemXXXXXX 115200`

## Core Programming Patterns

### Basic Program Structure

```cpp
#include "daisy_seed.h"

using namespace daisy;

DaisySeed hw;

// Audio callback (runs at sample rate, e.g., 48kHz)
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        // Process left channel
        out[0][i] = in[0][i];

        // Process right channel
        out[1][i] = in[1][i];
    }
}

int main(void)
{
    // Initialize hardware
    hw.Init();

    // Configure audio
    hw.SetAudioBlockSize(48); // samples per block
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Start audio
    hw.StartAudio(AudioCallback);

    // Main loop
    while(1)
    {
        // Update controls, LEDs, etc.
        System::Delay(10);
    }
}
```

### Audio Processing Guidelines

- **Audio callback**: Must complete in < 1ms (for 48 sample blocks at 48kHz)
- **Avoid in callback**: Memory allocation, printing, blocking operations
- **Use**: Pre-allocated buffers, lookups tables, optimized DSP
- **Optimization**: Enable `-O2` or `-O3` in Makefile

### GPIO and Controls

```cpp
// LED control
hw.SetLed(true);  // Turn on onboard LED

// ADC inputs (pots, CV)
AdcChannelConfig adc_config;
adc_config.InitSingle(seed::A0);
hw.adc.Init(&adc_config, 1);
hw.adc.Start();

// In main loop
float pot_value = hw.adc.GetFloat(0);  // 0.0 to 1.0
```

### DaisySP Effects

```cpp
#include "daisysp.h"

using namespace daisysp;

Reverb reverb;
Overdrive overdrive;

void Init() {
    hw.Init();

    // Initialize DSP
    reverb.Init(hw.AudioSampleRate());
    reverb.SetFeedback(0.85f);
    reverb.SetLpFreq(8000.0f);

    overdrive.Init();
    overdrive.SetDrive(0.7f);
}

void AudioCallback(...) {
    for(size_t i = 0; i < size; i++) {
        float dry = in[0][i];
        float wet = overdrive.Process(dry);
        wet = reverb.Process(wet);
        out[0][i] = wet;
    }
}
```

## Memory Management

### Memory Regions

- **Internal SRAM**: 512KB (fast, for critical audio buffers)
- **SDRAM**: 64MB (slower, for large buffers like delay lines)
- **QSPI Flash**: 8MB (program storage, accessed via bootloader)

### Using SDRAM

```cpp
// Allocate in SDRAM for large buffers
#define DELAY_BUFFER_SIZE 96000  // 2 seconds at 48kHz

float DSY_SDRAM_BSS delay_buffer[DELAY_BUFFER_SIZE];
```

### Stack Size

Increase stack if needed in linker script or with:

```makefile
LDFLAGS += -Wl,--defsym=_Min_Stack_Size=0x4000
```

## MIDI Integration

### USB MIDI

```cpp
#include "usb_midi.h"

MidiUsbHandler midi;

void HandleMidiMessage(MidiEvent m) {
    switch(m.type) {
        case NoteOn:
            // Handle note on
            break;
        case ControlChange:
            // Handle CC
            break;
    }
}

void Init() {
    hw.Init();

    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);
}

void Loop() {
    midi.Listen();
    while(midi.HasEvents()) {
        HandleMidiMessage(midi.PopEvent());
    }
}
```

### SysEx Messages

```cpp
uint8_t sysex_buffer[256];
size_t sysex_length;

void HandleSysEx(uint8_t* data, size_t len) {
    if(len < 3) return;
    if(data[0] != 0xF0) return;  // Check start byte

    uint8_t mfg_id = data[1];    // Manufacturer ID
    uint8_t command = data[2];    // Command byte

    // Process command
    switch(command) {
        case 0x10:
            // Handle SET_PATCH
            break;
        case 0x12:
            // Handle GET_PATCH
            break;
    }
}
```

## Common Issues and Solutions

### Build Errors

- **"No rule to make target"**: Check `CPP_SOURCES` paths in Makefile
- **"undefined reference to"**: Missing source file or library link
- **Stack overflow**: Increase `_Min_Stack_Size` in linker script

### Flash Errors

- **"Cannot open DFU device"**: Not in bootloader mode, try BOOT+RESET sequence again
- **"No DFU capable device"**: Check USB cable (needs data lines), try different port
- **Mac permission issues**: May need to run with `sudo` or configure udev rules

### Audio Issues

- **Noise/crackling**: Audio callback taking too long, optimize code or reduce block size
- **No audio output**: Check `AudioHandle` configuration, sample rate, and callback registration
- **Pops on parameter changes**: Use smoothing/slew limiting in parameter updates

### Debugging Issues

- **Can't connect with openocd**: Check ST-Link connection, ensure red stripe orientation
- **Breakpoints not working**: Rebuild with debug symbols: `make clean && make`

## Best Practices

### Performance

1. **Optimize hot paths**: Audio callback must be fast
2. **Use lookup tables**: For expensive math (sin, exp, etc.)
3. **Minimize branching**: Use branchless code in audio callback
4. **Profile code**: Use GPIO toggles to measure timing
5. **Use DMA**: For ADC, DAC, and other peripherals

### Code Organization

1. **Separate concerns**: Audio processing, UI, MIDI in different modules
2. **Pre-allocate**: All buffers and objects before starting audio
3. **Use const**: For lookup tables and configuration
4. **Static analysis**: Use `-Wall -Wextra` compiler flags

### Testing

1. **Incremental development**: Test each feature individually
2. **Serial debugging**: Use `PrintLine()` for debugging (not in audio callback)
3. **Debug builds**: Temporarily disable optimization for debugging
4. **Passthrough test**: Verify audio path with simple passthrough

## Resources

### Documentation

- **libDaisy API**: https://daisy.audio/libDaisy/
- **DaisySP API**: https://daisy.audio/DaisySP/
- **Tutorials**: https://daisy.audio/tutorials/
- **Examples**: https://github.com/electro-smith/DaisyExamples

### Hardware

- **Daisy Seed Datasheet**: Pin mappings, specs
- **STM32H750 Reference Manual**: Chip documentation
- **Debug Probe**: ST-Link V3 mini recommended

### Community

- **Daisy Forum**: https://forum.electro-smith.com/
- **Discord**: Community support and discussion

## Project-Specific Notes

This project (DaisyMultiFX) implements:

- **12-slot audio routing**: Each slot can route from physical input or another slot
- **Self-describing effects**: Effects expose metadata via `EffectMeta`
- **MIDI SysEx control**: Patch management via USB MIDI
- **Tempo sync**: Tap tempo functionality for time-synced effects
- **VST wrapper**: Parallel VST3 implementation for testing

### Build Commands for This Project

```bash
cd firmware
make clean
make -j
make flash

# Debug build with passthrough patch
make clean
make -j DEBUG_PATCH=1
make flash DEBUG_PATCH=1
```

### Effect Development

See [docs/adding-effects.md](docs/adding-effects.md) for guidelines on adding new effects to this project.
