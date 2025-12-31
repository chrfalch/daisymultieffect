# Architecture (DaisyMultiFX)

This document describes the overall architecture of the **DaisyMultiFX** system:
- **Core DSP library** (platform-agnostic effects and audio processing)
- **Daisy Seed firmware** (real-time audio + MIDI SysEx endpoint)
- **VST Plugin** (for testing and development in a DAW)
- **iOS/iPad controller app** (SwiftUI) that configures patches and receives state updates
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
│   └── midi/                # SysEx utilities (no transport)
├── firmware/                # Daisy Seed specific code
│   └── src/                 # main.cpp, hardware I/O, SDRAM buffers
├── vst/                     # VST3 plugin for DAW testing
│   └── src/                 # Plugin processor, buffer manager
└── host/                    # iOS/macOS controller apps
```

### Components
1. **Daisy Seed Pedal (Firmware)**
   - Real-time audio processing: multi-effect “pedalboard”
   - Reads hardware controls:
     - Footswitch 1/2 (toggle/tap-tempo, per patch)
     - (future) knobs/encoders, expression pedal, LEDs
   - Communicates with host (Mac/iPad) via **MIDI** (preferably **USB MIDI**)
     - Receives: patches, parameter edits, metadata queries
     - Sends: button state changes, tempo updates, patch dumps, metadata responses

2. **Host / Controller App (iOS/iPad)**
   - UI for:
     - Listing available effects (from firmware metadata)
     - Constructing/ordering/routing effects into a patch (“pedalboard”)
     - Editing parameters
     - Saving/loading presets, scenes, etc.
   - Sends SysEx messages to the Daisy
   - Receives asynchronous SysEx events (tempo updates, button state changes)

3. **Transport**
   - **MIDI SysEx** using manufacturer ID `0x7D` (non-commercial)
   - Message payloads are intended to be **7-bit safe**
     - Some values are packed as Q16.16 into 5×7-bit bytes

---

## 2. Firmware architecture

### 2.1 Execution model
Firmware runs two “worlds”:
- **Audio callback (hard real-time)**
  - Must be deterministic, fixed cost, no allocation, no blocking I/O.
  - Processes audio sample frames (or blocks) through the effect graph.
- **Control / main loop (soft real-time)**
  - Polls MIDI
  - Polls buttons/knobs
  - Builds the “next patch” and schedules patch switching
  - Sends SysEx responses/notifications

**Not finished yet**
- In this starter repo, `firmware/src/main.cpp` is a skeleton and does **not** integrate libDaisy’s real AudioCallback/MIDI handlers.
- Patch switching is “immediate” in the skeleton; production should use a safe handover mechanism (see §2.6).

### 2.2 Core data model

#### Slot graph
The pedalboard is modeled as **N fixed slots** (currently `N=12`). Each slot contains:
- A pointer to an effect instance (`BaseEffect* effect`)
- Configuration:
  - `typeId` (which effect type)
  - `enabled` (bypass state)
  - `inputL`, `inputR` (routing taps)
    - `ROUTE_INPUT = 255` means “take from physical input”
    - otherwise, take from another slot’s output buffer
  - `sumToMono` (optional mono sum before processing)
  - `dry/wet` mix per slot (0..1)
  - `channelPolicy` (Auto / ForceMono / ForceStereo)
  - parameter list (up to 8 per slot in `PatchWireDesc`)

#### Patch description
A patch is described by `PatchWireDesc` (`firmware/src/patch_protocol.h`):
- `numSlots`
- `slots[]`: list of `SlotWireDesc`
- `buttons[2]`: per-patch footswitch assignments:
  - `ToggleBypass(slotIndex)`
  - `TapTempo` (global BPM)

**Not finished yet**
- Serialization/deserialization of a patch into SysEx payload is not implemented fully.
- The struct layout is an in-memory representation; you’ll likely define a compact binary layout for transport.

### 2.3 Effect interface and metadata

All effects implement `BaseEffect` (`firmware/src/base_effect.h`):
- `Init(sampleRate)`
- `ProcessStereo(float& l, float& r)`
- `SetParam(id, normalized0to1)`
- `GetParamsSnapshot(out[])` (returns id/value pairs 0..127)
- `GetMetadata()` returns `EffectMeta`:
  - name + description
  - parameter list:
    - `Number` with min/max/step
    - `Enum` with named options

#### Tempo-synced effects
`TimeSyncedEffect` (`firmware/src/time_synced_effect.h`) is a base class for effects that can be:
- free-time (ms) **or**
- tempo-synced using `TempoSource` (BPM) and a division index

**Not finished yet**
- Some metadata parameters (free time/division/synced) are currently “generic” (no explicit ranges) and can be improved.

### 2.4 Included effects (starter set)
Implemented as example code (not necessarily DSP-optimal):
- `DelayEffect` (tempo-synced)
- `StereoSweepDelayEffect` (tempo-synced delay with pan LFO)
- `DistortionEffect` (tanh clip + tone)
- `StereoMixerEffect` (combine two branches)
- `SimpleReverbEffect` (Schroeder-style comb+allpass tank)

### 2.5 Effect registry
`EffectRegistry` (`firmware/src/effect_registry.*`) provides:
- `Lookup(typeId) -> EffectMeta*`
Used to answer SysEx “metadata queries” from the iPad.

**Not finished yet**
- Registry should eventually include:
  - allocation/pool hooks (not only metadata)
  - versioning and capabilities

### 2.6 Patch switching and safety
To avoid glitches/tearing:
- Control thread should build a `next_patch` structure off-thread.
- Audio callback should:
  1. check if `next_patch` is available at a safe boundary
  2. optionally crossfade between `current_patch` and `next_patch` over X samples
  3. swap pointers atomically

**Not finished yet**
- Crossfade patch switching is not implemented in the starter code.
- The current `ApplyPatch()` pattern is not audio-safe as-is.

### 2.7 Hardware controls
Planned behavior:
- Buttons:
  - patch-defined bindings (toggle bypass / tap tempo)
  - send button/tempo updates to iPad
- Knobs/encoders (future):
  - map to params and/or send updates over MIDI

**Not finished yet**
- GPIO + debounce + LED feedback is not implemented.

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

### 3.3 7-bit safe packing
SysEx data bytes must be 0..127.
Strategy used in the starter:
- floats like BPM are converted to Q16.16 (signed int32)
- packed into 5 bytes, each holding 7 bits (`sysex_utils.h`)

**Not finished yet**
- Patch serialization must be defined to ensure 7-bit safety.
- If you want raw binary, you’ll need an escaping/packing scheme.

### 3.4 Versioning and compatibility (recommended)
Not implemented yet, but recommended:
- Add protocol version + firmware version
- Host adapts when:
  - effect types change
  - parameter IDs change
  - new effects appear

---

## 4. iOS/iPad app architecture

### 4.1 MIDI layer
`host/apple/MidiController.swift`
- Uses CoreMIDI
- Sends SysEx messages
- Receives SysEx and parses:
  - tempo updates
  - button state changes

**Not finished yet**
- Device discovery is naive (first source/destination).
- Add:
  - selecting the correct endpoint
  - reconnect
  - SysEx reassembly robustness (messages may be split)

### 4.2 Patch model (host side)
Recommended (not fully implemented in starter):
- `EffectType` (from `EffectMeta`)
- `EffectInstance` (typeId + parameter values)
- `PatchGraph` (slots + routing edges)

Host responsibilities:
- Build a patch graph in UI
- Compile into a patch payload
- Send `SET_PATCH`

### 4.3 UI generation from metadata
Because effects are self-describing:
- sliders for number params
- pickers for enum params
- auto labels and help text from metadata

**Not finished yet**
- Metadata parsing + dynamic UI generation is not implemented in starter UI.

---

## 5. End-to-end flows

### 5.1 Boot + discovery
1. iPad connects to Daisy MIDI endpoint
2. iPad sends `GET_ALL_EFFECT_META`
3. Daisy replies with `EFFECT_META_RESPONSE` messages
4. iPad builds the effect palette

### 5.2 Editing a patch
1. user edits chain/routing
2. app compiles a patch payload
3. app sends `SET_PATCH`
4. firmware applies patch (ideally with safe swap + crossfade)

### 5.3 Footswitch interaction
1. user presses hardware button
2. firmware toggles bypass or updates BPM
3. firmware sends `BUTTON_STATE_CHANGE` / `TEMPO_UPDATE`
4. iPad updates UI instantly

---

## 6. What’s incomplete in this monorepo starter

### Firmware
- Real libDaisy integration (Seed init + audio callback + USB MIDI)
- Full SysEx command set and patch encoding/decoding
- Audio-safe patch switching (atomic swap + crossfade)
- CPU timing/profiling in callback
- Hardware IO (buttons/LEDs/knobs) + debounce

### iOS
- Endpoint selection + reconnect
- Full `EffectMeta` parsing and dynamic UI generation
- Patch editor UI (graph + routing)
- Patch compiler/serializer

---

## 7. Suggested next steps (implementation order)
1. Add USB MIDI receive/send on Daisy (SysEx round-trip “ping” test)
2. Finalize patch binary schema (versioned + 7-bit safe)
3. Implement safe patch switching (next_patch + crossfade)
4. Implement metadata parsing on iPad (build palette + parameter UI)
5. Build patch editor + save/load presets/scenes
