# VST Plugin

This folder contains a VST3 plugin implementation that uses the shared `core/` DSP library.

## Purpose

- Test the effect chain in a DAW environment
- Develop and iterate on effects without hardware
- A/B compare firmware behavior with the plugin

## Building

The plugin uses [JUCE](https://juce.com/) framework for cross-platform VST3/AU support.

### Prerequisites

1. Download JUCE from https://juce.com/get-juce
2. Install CMake 3.15+
3. A C++17 compatible compiler

### Build Steps

```bash
cd vst
mkdir build && cd build
cmake .. -DJUCE_DIR=/path/to/JUCE
cmake --build . --config Release
```

### Without JUCE (Minimal VST3 SDK)

Alternatively, you can build directly against the VST3 SDK:

```bash
# Clone VST3 SDK
git clone --recursive https://github.com/steinbergmedia/vst3sdk.git

# Build
mkdir build && cd build
cmake .. -DVST3_SDK_ROOT=/path/to/vst3sdk
cmake --build . --config Release
```

## Architecture

```
vst/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── PluginProcessor.h   # Audio processing (uses core/AudioProcessor)
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h      # GUI (optional)
│   ├── PluginEditor.cpp
│   └── BufferManager.h     # Heap-allocated buffers for delays/reverbs
└── README.md
```

## Features

- Full effect chain from firmware
- Tempo sync from DAW
- Preset management via DAW
- Same patch format as firmware (can import/export)
