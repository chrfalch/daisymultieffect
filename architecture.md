# Architecture (DaisyMultiFX)

This document describes the overall architecture of the **DaisyMultiFX** system:
- **Core DSP library** (platform-agnostic effects and audio processing)
- **Daisy Seed firmware** (real-time audio + MIDI SysEx endpoint)
- **VST Plugin** (for testing and development in a DAW)
- **iOS/iPad controller app** (Expo/React Native) that configures patches and receives state updates
- **MIDI Bridge utility** (macOS app for routing Network MIDI to Hardware MIDI)
- The **SysEx protocol** that ties them together

It also marks which parts are **starter/skeleton** and still need finishing.

---

## 1. System overview

### Code Organization

```
daisyseed-multi-effect/
├── core/                    # Platform-agnostic DSP (shared code)
│   ├── audio/               # AudioProcessor, pedalboard, tempo
│   ├── effects/             # All effect implementations
│   ├── patch/               # Patch protocol definitions
│   ├── protocol/            # MIDI/SysEx protocol definitions
│   └── state/               # PatchState and observers
├── firmware/                # Daisy Seed specific code
│   └── src/                 # main.cpp, hardware I/O, SDRAM buffers
├── vst/                     # VST3/AU plugin for DAW testing
│   └── src/                 # Plugin processor, editor, buffer manager
├── bridge/                  # MIDI Bridge utility (macOS)
│   └── src/                 # Standalone MIDI routing app
└── host/                    # iOS/macOS controller apps
    └── expo/                # Expo/React Native app with native MIDI module
```

### Components
1. **Daisy Seed Pedal (Firmware)**
   - Real-time audio processing: multi-effect "pedalboard"
   - Reads hardware controls:
     - Footswitch 1/2 (toggle/tap-tempo, per patch)
     - (future) knobs/encoders, expression pedal, LEDs
   - Communicates with host (Mac/iPad) via **MIDI** (preferably **USB MIDI**)
     - Receives: patches, parameter edits, metadata queries
     - Sends: button state changes, tempo updates, patch dumps, metadata responses

2. **Host / Controller App (iOS/iPad)**
   - Built with Expo/React Native + native Swift MIDI module
   - UI for:
     - Listing available effects (from firmware metadata)
     - Constructing/ordering/routing effects into a patch ("pedalboard")
     - Editing parameters
     - Saving/loading presets, scenes, etc.
   - Connects via Network MIDI (when using Mac as bridge) or direct USB MIDI
   - Sends SysEx messages to the Daisy
   - Receives asynchronous SysEx events (tempo updates, button state changes)

3. **VST Plugin**
   - Full effect processing matching firmware behavior
   - Used for development/testing without hardware
   - Supports MIDI I/O for SysEx communication
   - Can run standalone or in a DAW

4. **MIDI Bridge (macOS utility)**
   - Lightweight standalone app for routing MIDI
   - Bridges Network MIDI (iPad) to USB MIDI (Daisy Seed)
   - No audio processing - purely MIDI routing
   - Useful when iPad cannot connect directly to Daisy via USB

5. **Transport**
   - **MIDI SysEx** using manufacturer ID `0x7D` (non-commercial)
   - Message payloads are intended to be **7-bit safe**
     - Some values are packed as Q16.16 into 5x7-bit bytes

---

## 2. Firmware architecture

### 2.1 Execution model
Firmware runs two "worlds":
- **Audio callback (hard real-time)**
  - Must be deterministic, fixed cost, no allocation, no blocking I/O.
  - Processes audio sample frames (or blocks) through the effect graph.
- **Control / main loop (soft real-time)**
  - Polls MIDI
  - Polls buttons/knobs
  - Builds the "next patch" and schedules patch switching
  - Sends SysEx responses/notifications

### 2.2 Core data model

#### Slot graph
The pedalboard is modeled as **N fixed slots** (currently `N=12`). Each slot contains:
- A pointer to an effect instance (`BaseEffect* effect`)
- Configuration:
  - `typeId` (which effect type)
  - `enabled` (bypass state)
  - `inputL`, `inputR` (routing taps)
    - `ROUTE_INPUT = 255` means "take from physical input"
    - otherwise, take from another slot's output buffer
  - `sumToMono` (optional mono sum before processing)
  - `dry/wet` mix per slot (0..1)
  - `channelPolicy` (Auto / ForceMono / ForceStereo)
  - parameter list (up to 8 per slot)

---

## 3. MIDI SysEx protocol

### 3.1 Goals
- Host can query available effects + parameters dynamically.
- Host can send complete patches (routing + params) to firmware.
- Firmware can send back:
  - patch dumps
  - tempo and button events
  - metadata responses

### 3.2 Message types (current plan)
Manufacturer `0x7D`:

- `SET_PATCH` (0x10): `F0 7D 10 <patch-bytes...> F7`
- `PATCH_DUMP_RESPONSE` (0x11): `F0 7D 11 <patch-bytes...> F7`
- `GET_PATCH` (0x12): `F0 7D 12 F7`

- `GET_EFFECT_META` (0x30): `F0 7D 30 <typeId> F7`
- `EFFECT_META_RESPONSE` (0x31): `F0 7D 31 <payload...> F7`
- `GET_ALL_EFFECT_META` (0x32): `F0 7D 32 F7`

- `BUTTON_STATE_CHANGE` (0x40): `F0 7D 40 <buttonIndex> <slotIndex> <enabled0or1> F7`
- `TEMPO_UPDATE` (0x41): `F0 7D 41 <bpm_Q16_16_5bytes> F7`

---

## 4. Building

### VST Plugin
```bash
cd vst
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
```

### MIDI Bridge
```bash
cd bridge
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
# App will be in: build/DaisyMidiBridge_artefacts/Daisy MIDI Bridge.app
```

### Firmware
```bash
cd firmware
make
make flash  # to upload to Daisy Seed
```

### iOS App
```bash
cd host/expo
npm install
npx expo run:ios
```

---

## 5. Network MIDI Setup (iPad to Daisy via Mac)

When using the iPad app to control the Daisy Seed hardware through a Mac:

1. **On Mac**: Open Audio MIDI Setup > MIDI Network Setup
2. Enable a session (e.g., "Session 1")
3. **On iPad**: Run the app - it will appear in the Mac's "Sessions and Directories"
4. Click "Connect" on the Mac to establish the Network MIDI link
5. **Run the MIDI Bridge app** on the Mac
6. Select "Session 1" as Network MIDI and "Daisy Seed Built In" as Hardware MIDI
7. Click "Connect" - the bridge will route MIDI bidirectionally

Alternatively, connect the iPad directly to Daisy Seed via USB using a Lightning/USB-C adapter.
