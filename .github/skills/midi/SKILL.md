---
name: daisy-seed-midi
description: Contains tools and documentation for how MIDI messages are created and managed within the DaisySeed Multi-Effect pedal project. This also includes the midi protocol based on sysex messages.
---

# MIDI Protocol for DaisySeed Multi-Effect

This document describes the custom SysEx-based MIDI protocol used for communication between the firmware, VST plugin, and mobile apps (iPad/iOS).

## Architecture Overview

```
┌─────────────┐     USB-MIDI      ┌─────────────┐
│   iPad App  │◄─────────────────►│  Firmware   │
│   (Swift)   │                   │  (Daisy)    │
└─────────────┘                   └─────────────┘
       ▲                                ▲
       │ IAC Driver / Virtual MIDI      │
       ▼                                │
┌─────────────┐                         │
│  VST/AU     │◄────────────────────────┘
│  Plugin     │   (can also connect via USB)
└─────────────┘
```

## Key Files

| File | Purpose |
|------|---------|
| [core/protocol/sysex_protocol_c.h](../../../core/protocol/sysex_protocol_c.h) | C-compatible protocol constants (usable by Swift via bridging) |
| [core/protocol/sysex_protocol.h](../../../core/protocol/sysex_protocol.h) | C++ constexpr wrappers around the C definitions |
| [core/protocol/midi_protocol.h](../../../core/protocol/midi_protocol.h) | C++ encoder/decoder functions for all message types |
| [firmware/src/midi/midi_control.cpp](../../../firmware/src/midi/midi_control.cpp) | Firmware SysEx message handler |
| [firmware/src/midi/sysex_utils.h](../../../firmware/src/midi/sysex_utils.h) | Firmware utility functions (Q16.16 packing) |
| [vst/src/PluginProcessor.cpp](../../../vst/src/PluginProcessor.cpp) | VST SysEx message handler |

## Message Format

All messages use manufacturer ID `0x7D` (educational/development use, non-registered).

### Basic Structure

```
F0 7D <sender> <cmd> [data...] F7
│  │     │       │       │      └─ SysEx end
│  │     │       │       └─ Command-specific payload
│  │     │       └─ Command code
│  │     └─ Sender ID (for loopback filtering)
│  └─ Manufacturer ID
└─ SysEx start
```

### Sender IDs

Each client type has a unique sender ID to enable loopback filtering (important when using IAC Driver):

| ID | Client |
|----|--------|
| `0x01` | Firmware |
| `0x02` | VST/AU Plugin |
| `0x03` | Swift/iOS App |
| `0x00` | Unknown |

## Commands (Host → Device)

### Slot Commands

| Command | Code | Format | Description |
|---------|------|--------|-------------|
| SET_PARAM | `0x20` | `<slot> <paramId> <value>` | Set effect parameter (0-127) |
| SET_ENABLED | `0x21` | `<slot> <enabled>` | Enable/disable slot (0/1) |
| SET_TYPE | `0x22` | `<slot> <typeId>` | Change effect type |
| SET_ROUTING | `0x23` | `<slot> <inputL> <inputR>` | Set input routing |
| SET_SUM_TO_MONO | `0x24` | `<slot> <sumToMono>` | Set mono summing (0/1) |
| SET_MIX | `0x25` | `<slot> <dry> <wet>` | Set dry/wet mix (0-127 each) |
| SET_CHANNEL_POLICY | `0x26` | `<slot> <policy>` | Set channel policy |

### Global Commands

| Command | Code | Format | Description |
|---------|------|--------|-------------|
| SET_INPUT_GAIN | `0x27` | `<gainDb_Q16.16_5B>` | Set input gain (0 to +24 dB) |
| SET_OUTPUT_GAIN | `0x28` | `<gainDb_Q16.16_5B>` | Set output gain (-12 to +12 dB) |
| REQUEST_PATCH | `0x12` | (no payload) | Request full patch dump |
| REQUEST_META | `0x32` | (no payload) | Request effect metadata |

## Responses (Device → Host)

| Response | Code | Description |
|----------|------|-------------|
| PATCH_DUMP | `0x13` | Full patch state (12 slots + buttons + gains) |
| EFFECT_META_V5 | `0x38` | Effect metadata with params, descriptions, ranges |
| BUTTON_STATE | `0x40` | Hardware button state change |
| TEMPO_UPDATE | `0x41` | BPM update (Q16.16 encoded) |
| STATUS_UPDATE | `0x42` | Input/output levels + CPU load |

## Data Encoding

### 7-Bit Safety

MIDI SysEx requires all data bytes to be 7-bit (0x00-0x7F). Values >= 0x80 must be encoded.

### Route Values

Route values indicate signal sources:
- `0-11`: Output from slot N
- `255` (`SYSEX_ROUTE_INPUT`): Hardware input

Wire encoding: `255 → 127`, all others unchanged.

```c
// Encode for transmission
uint8_t encoded = (route == 255) ? 127 : (route & 0x7F);

// Decode on receipt
uint8_t route = (encoded == 127) ? 255 : encoded;
```

### Q16.16 Fixed-Point

Float values (tempo, gain, levels) use Q16.16 fixed-point encoding packed into 5 bytes:

```c
// Float to Q16.16
int32_t q = (int32_t)(floatValue * 65536.0f + 0.5f);

// Pack to 5 bytes (7-bit safe)
out[0] =  q        & 0x7F;
out[1] = (q >> 7)  & 0x7F;
out[2] = (q >> 14) & 0x7F;
out[3] = (q >> 21) & 0x7F;
out[4] = (q >> 28) & 0x7F;

// Unpack from 5 bytes
int32_t q = in[0] | (in[1] << 7) | (in[2] << 14) | (in[3] << 21) | (in[4] << 28);

// Q16.16 to float
float value = (float)q / 65536.0f;
```

## Effect Type IDs

| ID | Effect |
|----|--------|
| `0` | Off (passthrough) |
| `1` | Delay |
| `10` | Distortion/Overdrive |
| `12` | Stereo Sweep Delay |
| `13` | Stereo Mixer |
| `14` | Reverb |
| `15` | Compressor |
| `16` | Chorus |
| `17` | Noise Gate |
| `18` | Graphic EQ |
| `19` | Flanger |
| `20` | Phaser |
| `21` | Neural Amp |
| `22` | Cabinet IR |

## Effect Metadata V5 Format (0x38)

The V5 format provides complete effect metadata including parameter descriptions, units, ranges, and enum options:

```
F0 7D <sender> 38 <typeId> <nameLen> <name...>
  <shortName[3]> <descLen> <description...>
  <numParams>
  [for each param:]
    <paramId> <kind> <flags> <nameLen> <name...>
    <descLen> <description...>
    <unitPrefixLen> <unitPrefix...>
    <unitSuffixLen> <unitSuffix...>
    [if flags & 0x01: NumberRange]
    [if flags & 0x02: EnumOptions]
F7
```

### Parameter Kind Values
- `0`: Number (continuous 0-127 value)
- `1`: Enum (discrete selection)
- `2`: File (reserved for future use)

### Flags Byte
- Bit 0 (`0x01`): Has NumberRange (min/max/step as Q16.16)
- Bit 1 (`0x02`): Has EnumOptions (dropdown choices)

### NumberRange (15 bytes, if flags & 0x01)
```
<min_Q16.16_5B> <max_Q16.16_5B> <step_Q16.16_5B>
```

### EnumOptions (variable, if flags & 0x02)
```
<numOptions>
[for each option:]
  <value> <nameLen> <name...>
```

Example enum options for Neural Amp model selector:
```
05                    // 5 options
00 09 TW40 Clean      // value=0, name="TW40 Clean"
01 0A TW40 Crunch     // value=1, name="TW40 Crunch"
02 0A TW40 Blues      // value=2, name="TW40 Blues"
03 0B TW40 Rhythm     // value=3, name="TW40 Rhythm"
04 09 TW40 Lead       // value=4, name="TW40 Lead"
```

## Patch Dump Format (0x13)

The patch dump is the most complex message, containing full system state:

```
F0 7D <sender> 13 <numSlots>
  [12x SlotDesc]
  [2x ButtonAssign]
  <inputGainDb_Q16.16_5B>
  <outputGainDb_Q16.16_5B>
F7
```

Each SlotDesc (26 bytes):
```
<slotIndex> <typeId> <enabled> <inputL> <inputR> <sumToMono>
<dry> <wet> <channelPolicy> <numParams>
[8x ParamPair: <id> <value>]
```

Each ButtonAssign (2 bytes):
```
<slotIndex> <mode>
```

## Adding New Protocol Commands

### 1. Define the Command Code

Add to [sysex_protocol_c.h](../../../core/protocol/sysex_protocol_c.h):

```c
enum {
    // ... existing commands ...

    /** My new command: F0 7D <sender> XX <payload...> F7 */
    SYSEX_CMD_MY_NEW_COMMAND = 0xXX,
};
```

### 2. Add C++ Wrapper (Optional)

Add to [sysex_protocol.h](../../../core/protocol/sysex_protocol.h):

```cpp
namespace SysExProtocol {
    namespace Cmd {
        static constexpr uint8_t MY_NEW_COMMAND = SYSEX_CMD_MY_NEW_COMMAND;
    }
}
```

### 3. Add Encoder/Decoder

Add to [midi_protocol.h](../../../core/protocol/midi_protocol.h):

```cpp
// In DecodedMessage struct - add fields for the new command's data
struct DecodedMessage {
    // ... existing fields ...
    float myNewValue = 0.0f;  // For MY_NEW_COMMAND
};

// Encoder
inline std::vector<uint8_t> encodeMyNewCommand(uint8_t sender, float value)
{
    std::vector<uint8_t> msg = {0xF0, MANUFACTURER_ID, sender, Cmd::MY_NEW_COMMAND};
    uint8_t q[5];
    packQ16_16(floatToQ16_16(value), q);
    msg.insert(msg.end(), q, q + 5);
    msg.push_back(0xF7);
    return msg;
}

// In decode() function - add case for the new command
case Cmd::MY_NEW_COMMAND:
    if (size >= 4 + 5) {  // sender + cmd + 5 bytes payload
        result.myNewValue = unpackQ16_16(&data[4]);
        result.valid = true;
    }
    break;
```

### 4. Handle in Firmware

Add to [midi_control.cpp](../../../firmware/src/midi/midi_control.cpp) `HandleSysexMessage()`:

```cpp
case 0xXX: // MY_NEW_COMMAND
{
    float value = unpackQ16_16(&bytes[payload]);
    // Queue for audio thread if needed
    pending_my_value_q16_ = floatToQ16_16(value);
    pending_ |= PendingKind::MyValue;
    break;
}
```

### 5. Handle in VST

Add to [PluginProcessor.cpp](../../../vst/src/PluginProcessor.cpp) `handleIncomingMidi()`:

```cpp
case MidiProtocol::Cmd::MY_NEW_COMMAND:
    patchState_.setMyValue(decoded.myNewValue);
    break;
```

### 6. Update State Management

If the command modifies persistent state, add to:
- [patch_state.h](../../../core/state/patch_state.h): State variable + setter/getter
- [patch_observer.h](../../../core/state/patch_observer.h): Callback for state changes

## Thread Safety

### Firmware

MIDI is received on main thread, audio runs on interrupt. Use pending flags:

```cpp
// In main thread (MIDI handler):
pending_value_ = newValue;
pending_ |= PendingKind::Value;

// In audio thread (ProcessFrame):
if (pending_ & PendingKind::Value) {
    actualValue_ = pending_value_;
    pending_ &= ~PendingKind::Value;
}
```

### VST

PatchState uses deduplication to prevent feedback loops. When receiving MIDI:

```cpp
// PatchState checks if value changed before notifying observers
void setValue(float v) {
    if (value_ == v) return;  // Deduplicate
    value_ = v;
    notifyValueChanged(v);
}
```

## Debugging Tips

1. **Enable logging**: Both firmware and VST print received commands to console
2. **Check sender ID**: Loopback issues manifest as "ignored own message" logs
3. **Verify 7-bit encoding**: Any byte >= 0x80 in payload corrupts the message
4. **Test Q16.16**: Print both packed bytes and unpacked float to verify encoding

## Common Pitfalls

1. **Forgetting F0/F7**: SysEx must start with 0xF0 and end with 0xF7
2. **Wrong sender ID**: Always include sender to enable loopback filtering
3. **Route encoding**: Remember 255 encodes as 127 on the wire
4. **Payload offset**: Data starts at byte 4 (after F0, manufacturer, sender, cmd)
5. **Float precision**: Q16.16 has ~0.00002 precision, sufficient for audio params
