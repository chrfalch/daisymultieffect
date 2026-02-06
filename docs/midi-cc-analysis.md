# MIDI Control Change (CC) Support and Patch Storage Analysis

## Executive Summary

This report analyzes the current MIDI SysEx-based parameter control system in the DaisyMultiFX project and provides a comprehensive plan for adding standard MIDI Control Change (CC) support alongside patch storage capabilities.

### Key Findings
1. Current system uses custom SysEx protocol (manufacturer ID 0x7D)
2. All parameter control is via SysEx messages with 7-bit values (0-127)
3. 12 effect slots with up to 8 parameters each = 96 parameters max per patch
4. Memory available: ~7.9MB QSPI flash, 64MB SDRAM, 512KB SRAM
5. No persistent patch storage currently implemented

## 1. Current Architecture Analysis

### 1.1 MIDI Protocol Overview

**Files Analyzed:**
- `core/protocol/midi_protocol.h` (598 lines)
- `core/protocol/sysex_protocol.h` (190 lines)
- `firmware/src/midi/midi_control.cpp` (1043 lines)
- `firmware/src/midi/midi_control.h` (133 lines)

**Current Message Types (SysEx only):**

#### Commands (Host → Firmware):
- `REQUEST_PATCH (0x12)` - Query current patch
- `LOAD_PATCH (0x14)` - Load complete patch (all 12 slots)
- `SET_PARAM (0x20)` - Set single parameter: slot, paramId, value (0-127)
- `SET_ENABLED (0x21)` - Enable/disable effect slot
- `SET_TYPE (0x22)` - Change effect type in slot
- `SET_ROUTING (0x23)` - Configure signal routing
- `SET_SUM_TO_MONO (0x24)` - Configure mono summing
- `SET_MIX (0x25)` - Set dry/wet mix
- `SET_CHANNEL_POLICY (0x26)` - Set stereo/mono policy
- `SET_INPUT_GAIN (0x27)` - Set input gain (dB)
- `SET_OUTPUT_GAIN (0x28)` - Set output gain (dB)
- `SET_GLOBAL_BYPASS (0x29)` - Global bypass switch
- `REQUEST_META (0x32)` - Query effect metadata

#### Responses (Firmware → Host):
- `PATCH_DUMP (0x13)` - Complete patch state
- `EFFECT_META_V5 (0x38)` - Effect metadata with full param info
- `STATUS_UPDATE (0x42)` - CPU load, levels, output parameters
- `TEMPO_UPDATE (0x41)` - BPM updates
- `BUTTON_STATE (0x40)` - Hardware button states

**Message Format:**
```
F0 7D <sender> <cmd> <data...> F7
  ^   ^    ^      ^      ^     ^
  |   |    |      |      |     End of SysEx
  |   |    |      |      Command-specific data
  |   |    |      Command code (0x12, 0x20, etc.)
  |   |    Sender ID (0x01=firmware, 0x02=VST, 0x03=Swift)
  |   Manufacturer ID (educational/dev use)
  SysEx start
```

### 1.2 Parameter Handling Flow

**Current Flow:**
1. Controller app sends SysEx message (e.g., SET_PARAM)
2. `MidiControl::OnUsbMidiRx()` receives bytes, reassembles SysEx
3. `MidiControl::HandleSysexMessage()` parses and queues change
4. Audio callback calls `MidiControl::ApplyPendingInAudioThread()`
5. Change is applied to `AudioProcessor` and `PedalBoardRuntime`
6. DSP processes audio with new parameter value

**Thread Safety:**
- Main loop: Receives MIDI, queues parameter updates
- Audio ISR: Applies queued updates atomically
- Uses volatile 32-bit word packing for lockless transfer
- Format: `kind(8) | slot(8) | paramId(8) | value(8)`

### 1.3 Effect System

**Current Effects (19 total):**
- Off (0), Delay (1), Overdrive (10), Sweep Delay (12)
- Mixer (13), Reverb (14), Compressor (15), Chorus (16)
- Noise Gate (17), Graphic EQ (18), Flanger (19), Phaser (20)
- Neural Amp (21), Cabinet IR (22), Tremolo (23), Auto Wah (24)
- Pitch Shifter (25), Tuner (26), Leslie (27)

**Parameter Characteristics:**
- Each effect: 0-8 parameters
- Parameters stored as 7-bit values (0-127)
- Metadata includes: name, description, unit, min/max/step
- Parameter types: Number (float ranges) or Enum (named options)
- Max parameters per patch: 12 slots × 8 params = 96 parameters

## 2. Standard MIDI CC Protocol

### 2.1 MIDI CC Message Format

Standard MIDI Control Change messages are **channel messages** (not SysEx):

```
Status  Data1  Data2
0xBn    CC#    Value
  ^      ^      ^
  |      |      Value (0-127)
  |      Controller number (0-127)
  Channel (0-15, n = 0-F)
```

**Key Differences from SysEx:**
- 3 bytes vs. variable-length SysEx
- Real-time: Can be sent/received during audio playback
- Standardized: Works with any MIDI controller
- Channel-based: 16 independent channels
- Instant: No reassembly needed like SysEx

### 2.2 Standard CC Numbers

**Commonly Used:**
- CC 1: Modulation Wheel
- CC 7: Volume
- CC 10: Pan
- CC 11: Expression
- CC 64: Sustain Pedal (0-63 off, 64-127 on)
- CC 74-79: Sound Controllers
- CC 14-31: Continuous controllers (often undefined)
- CC 102-119: Undefined (available for custom use)

**Bank Select System:**
- CC 0: Bank Select MSB
- CC 32: Bank Select LSB
- Can select up to 16,384 banks × 128 patches = 2,097,152 combinations

### 2.3 Parameter Mapping Strategy

**Challenge:** Map 96+ parameters to 128 CC numbers

**Proposed Solution: Hybrid Approach**

1. **Direct Mapping** (Most-Used Parameters)
   - Assign frequently-adjusted parameters to fixed CCs
   - Examples:
     - CC 1: Slot 0 Param 0 (e.g., Delay Time)
     - CC 2: Slot 0 Param 1 (e.g., Delay Feedback)
     - CC 7: Output Gain
     - CC 11: Input Gain
     - CC 64-79: Effect on/off switches (per slot)

2. **Slot Selection Mode**
   - CC 100: Select Active Slot (0-11)
   - CC 101-108: Control parameters 0-7 of active slot
   - Controller workflow:
     1. Send CC 100 with value 3 (select slot 3)
     2. Send CC 101 with value 64 (set param 0 of slot 3)

3. **Bank Select for Patches**
   - CC 0: Patch Bank Select (0-127 = 128 banks)
   - Program Change (0-127 patches per bank)
   - Total: 16,384 patch slots

4. **NRPN (Non-Registered Parameter Numbers)**
   - For complex parameter addressing
   - Uses CC 98-101 for parameter selection
   - Allows access to all 12 × 8 = 96 parameters
   - Format:
     ```
     CC 99: NRPN MSB (slot number 0-11)
     CC 98: NRPN LSB (param ID 0-7)
     CC 6:  Data Entry MSB (value 0-127)
     ```

## 3. Patch Storage Design

### 3.1 Memory Analysis

**Available Memory:**
- QSPI Flash: 7936 KB (7.9 MB) - currently used for code/data
- SDRAM: 64 MB - used for delay buffers, audio processing
- SRAM: 512 KB internal - used for real-time operations
- BACKUP_SRAM: 4 KB battery-backed - ideal for current patch

**Patch Size Calculation:**
```
Per Patch:
- 12 slots × 26 bytes = 312 bytes
  - slotIndex (1) + typeId (1) + enabled (1)
  - inputL (1) + inputR (1) + sumToMono (1)
  - dry (1) + wet (1) + channelPolicy (1)
  - numParams (1) + 8×(paramId + value) (16)
- Button mappings: 2 × 2 = 4 bytes
- Gain values: 2 × 4 = 8 bytes (float)
- Metadata: 32 bytes (name, timestamp, etc.)
Total: ~360 bytes per patch
```

**Storage Capacity:**
- Using 1 MB of QSPI flash: 2,844 patches
- Using 256 KB: 711 patches
- Practical target: **128 patches (8 banks × 16)** = 46 KB

### 3.2 Proposed Storage Architecture

**Flash Layout:**
```
QSPI Flash (0x90040000 - 0x907C0000):
├── 0x90040000: Firmware code/data (current)
├── 0x90700000: Patch Storage Area (256 KB)
│   ├── Header (4 KB): Magic, version, patch count, bank names
│   ├── Bank 0 (16 patches × 512 bytes) = 8 KB
│   ├── Bank 1 (16 patches × 512 bytes) = 8 KB
│   ├── ...
│   └── Bank 7 (16 patches × 512 bytes) = 8 KB
└── 0x90740000: Reserved for future use
```

**Backup SRAM (4 KB):**
- Current active patch (512 bytes)
- Last used patch index (4 bytes)
- Quick recall buffer (512 bytes)
- Settings/calibration data (remaining ~3 KB)

### 3.3 Patch Management Features

**Core Features:**
1. **Persistent Storage**
   - Save current patch to flash
   - Load patch from flash
   - Initialize with factory defaults

2. **Patch Organization**
   - 8 banks × 16 patches = 128 total
   - Bank names (16 chars each)
   - Patch names (24 chars each)
   - Timestamp for last modified

3. **Quick Recall**
   - Last 4 patches in backup SRAM
   - Instant recall without flash read

4. **Factory Reset**
   - Restore factory patches from embedded data
   - User patches preserved in separate area

## 4. Coexistence Strategy: CC + SysEx

### 4.1 Design Principles

**Both protocols should coexist:**
1. **SysEx for Programming** (Controller App)
   - Full patch editing
   - Effect selection/routing
   - Metadata queries
   - Bidirectional sync
   - Status updates

2. **CC for Performance** (MIDI Controllers)
   - Real-time parameter control
   - Patch recall
   - Effect enable/disable
   - Expression control

### 4.2 Message Priority

**In Audio Callback:**
```c++
void AudioCallback() {
    // 1. Apply CC messages (lowest latency)
    midi_control.ApplyCCMessages();
    
    // 2. Apply SysEx parameter changes
    midi_control.ApplyPendingSysEx();
    
    // 3. Process audio
    processor.Process(in, out, size);
}
```

### 4.3 Conflict Resolution

**When both CC and SysEx modify same parameter:**

1. **Last-Write-Wins**
   - Track timestamp of last update per parameter
   - Most recent change takes precedence

2. **Bidirectional Sync**
   - When CC changes parameter → Send SysEx notification
   - When SysEx changes parameter → No CC echo (prevents loop)
   - Controller app sees both changes

3. **Takeover Modes**
   - **Pickup**: CC takes effect only when passing current value
   - **Immediate**: CC immediately changes parameter
   - **Scale**: CC scales relative to current value

## 5. Implementation Plan

### 5.1 Phase 1: CC Parameter Control (No Storage)

**Goal:** Add real-time CC support alongside existing SysEx

**Changes Required:**

1. **MIDI Receive Path** (`firmware/src/midi/midi_control.cpp`)
   ```cpp
   // Add CC message parser in OnUsbMidiRx()
   if ((data[i] & 0xF0) == 0xB0) {  // Control Change
       uint8_t channel = data[i] & 0x0F;
       uint8_t cc = data[i+1] & 0x7F;
       uint8_t value = data[i+2] & 0x7F;
       HandleCCMessage(channel, cc, value);
   }
   ```

2. **CC Mapping Table** (`core/protocol/cc_mapping.h` - NEW FILE)
   ```cpp
   struct CCMapping {
       uint8_t cc;
       uint8_t slot;
       uint8_t paramId;
       bool isGlobal;  // vs. slot-specific
   };
   
   // Default mapping
   constexpr CCMapping kDefaultCCMap[] = {
       {7, 0xFF, 0xFF, true},   // CC 7 → Output Gain
       {11, 0xFF, 0xFF, true},  // CC 11 → Input Gain
       {64, 0, 0xFF, false},    // CC 64 → Slot 0 Enable
       {65, 1, 0xFF, false},    // CC 65 → Slot 1 Enable
       // ...
   };
   ```

3. **CC Handler** (`firmware/src/midi/midi_control.cpp`)
   ```cpp
   void MidiControl::HandleCCMessage(uint8_t ch, uint8_t cc, uint8_t val) {
       // Lookup CC mapping
       const CCMapping* map = FindCCMapping(cc);
       if (!map) return;
       
       if (map->isGlobal) {
           // Handle global parameters
           if (cc == 7) SetOutputGain(val);
           if (cc == 11) SetInputGain(val);
       } else {
           // Handle slot-specific
           if (map->paramId == 0xFF) {
               // Enable/disable slot
               SetSlotEnabled(map->slot, val >= 64);
           } else {
               // Set parameter
               SetParameter(map->slot, map->paramId, val);
           }
       }
   }
   ```

4. **Configuration Commands** (NEW SysEx messages)
   ```
   SET_CC_MAPPING (0x50): F0 7D 01 50 <cc> <slot> <paramId> F7
   GET_CC_MAPPING (0x51): F0 7D 01 51 <cc> F7
   RESET_CC_MAPPING (0x52): F0 7D 01 52 F7
   ```

**Effort Estimate:** 2-3 days
**Risk:** Low - additive change, no breaking changes

### 5.2 Phase 2: Patch Storage

**Goal:** Store/recall up to 128 patches in QSPI flash

**Changes Required:**

1. **Flash Driver** (`firmware/src/storage/flash_storage.cpp` - NEW FILE)
   ```cpp
   class FlashStorage {
   public:
       bool Init();
       bool SavePatch(uint8_t bank, uint8_t num, const PatchWireDesc& patch);
       bool LoadPatch(uint8_t bank, uint8_t num, PatchWireDesc& patch);
       bool ErasePatch(uint8_t bank, uint8_t num);
       bool FormatStorage();  // Erase all, write header
   };
   ```

2. **Storage Layout** (`core/storage/storage_layout.h` - NEW FILE)
   ```cpp
   struct StorageHeader {
       uint32_t magic;        // 0xDA15YPAT
       uint16_t version;      // 1
       uint16_t numBanks;     // 8
       uint16_t patchesPerBank;  // 16
       uint32_t reserved[8];
   };
   
   struct PatchMetadata {
       char name[24];
       uint32_t timestamp;
       uint32_t crc32;        // Validate integrity
       uint16_t size;
       uint8_t bank;
       uint8_t number;
   };
   ```

3. **MIDI Commands** (NEW)
   ```
   SAVE_PATCH (0x60): F0 7D 01 60 <bank> <num> F7
   LOAD_PATCH_FROM_STORAGE (0x61): F0 7D 01 61 <bank> <num> F7
   DELETE_PATCH (0x62): F0 7D 01 62 <bank> <num> F7
   LIST_PATCHES (0x63): F0 7D 01 63 <bank> F7
   RENAME_PATCH (0x64): F0 7D 01 64 <bank> <num> <name...> F7
   ```

4. **Bank Select via CC**
   ```cpp
   // In HandleCCMessage:
   if (cc == 0) {  // Bank Select MSB
       current_bank_ = val & 0x7F;
   }
   ```

5. **Program Change Handler**
   ```cpp
   void HandleProgramChange(uint8_t channel, uint8_t program) {
       // Load patch from storage
       PatchWireDesc patch;
       if (storage_.LoadPatch(current_bank_, program, patch)) {
           LoadFullPatch(patch);
       }
   }
   ```

**Effort Estimate:** 4-5 days
**Risk:** Medium - requires flash driver, wear leveling

### 5.3 Phase 3: Advanced CC Features

**Goal:** Slot selection, NRPN, takeover modes

**Changes Required:**

1. **Slot Selection Mode**
   ```cpp
   uint8_t active_slot_ = 0;  // Selected slot for CC control
   
   if (cc == 100) {
       active_slot_ = std::min(val / 11, 11);  // CC 100: 0-127 → slots 0-11
   } else if (cc >= 101 && cc <= 108) {
       uint8_t paramId = cc - 101;
       SetParameter(active_slot_, paramId, val);
   }
   ```

2. **NRPN Support**
   ```cpp
   uint16_t nrpn_msb_ = 0, nrpn_lsb_ = 0;
   
   if (cc == 99) nrpn_msb_ = val;        // Slot
   else if (cc == 98) nrpn_lsb_ = val;   // Param ID
   else if (cc == 6) {                    // Data Entry
       uint8_t slot = nrpn_msb_;
       uint8_t paramId = nrpn_lsb_;
       if (slot < 12 && paramId < 8) {
           SetParameter(slot, paramId, val);
       }
   }
   ```

3. **Takeover Modes** (Per CC mapping)
   ```cpp
   enum class TakeoverMode {
       Immediate,  // Value jumps immediately
       Pickup,     // Must pass current value first
       Scale       // Relative scaling
   };
   ```

**Effort Estimate:** 2-3 days
**Risk:** Low - optional enhancements

## 6. Potential Issues and Solutions

### 6.1 SysEx vs. CC Conflicts

**Issue:** Controller app and MIDI controller both modify parameters

**Solution:**
- Bidirectional notification: CC changes trigger SysEx notifications
- Controller app receives all changes regardless of source
- Visual feedback shows "external control" indicator

### 6.2 Memory Constraints

**Issue:** 128 patches × 512 bytes = 64 KB seems large

**Solution:**
- Actual patch data: ~360 bytes
- 512 bytes per slot includes future expansion
- Can reduce to 128 bytes if needed (355 patches in 256 KB)
- Start with 32 patches (16 KB) for MVP

### 6.3 Flash Wear Leveling

**Issue:** Flash has limited write cycles (~10,000)

**Solution:**
- Implement wear leveling (distribute writes)
- Cache frequently-modified patches in RAM
- Batch writes (save on manual trigger, not auto)
- Expected lifetime: 10,000 writes / 10 patches/day = 2.7 years

### 6.4 Patch Recall Latency

**Issue:** Loading from flash during performance causes glitches

**Solution:**
- Preload next patch into buffer during idle
- Use "crossfade patch switching" (already planned)
- Keep last 4 patches in backup SRAM for instant recall

### 6.5 CC Mapping Complexity

**Issue:** 96 parameters > 128 CCs, how to map?

**Solution:**
- Default mapping for common parameters
- User-configurable mapping (stored per patch)
- Hybrid: Direct + Slot Selection + NRPN
- Controller app provides GUI for mapping setup

### 6.6 SysEx Message Ordering

**Issue:** Current system assumes one message at a time

**Solution:**
- Already implemented: 4-slot queue for SysEx messages
- CC messages processed immediately (no queue needed)
- Maintain separate CC and SysEx pending buffers

## 7. Testing Strategy

### 7.1 Unit Tests

**Core Protocol:**
- CC message parsing (various channels, CCs)
- CC-to-parameter mapping lookup
- NRPN assembly/disassembly
- Bank select + program change logic

**Storage:**
- Flash read/write operations
- Patch serialization/deserialization
- CRC validation
- Wear leveling algorithm

### 7.2 Integration Tests

**MIDI Flow:**
1. Send CC → Verify parameter change → Check DSP output
2. Send CC → Verify SysEx notification sent to app
3. Send SysEx → Verify no CC echo
4. Interleave CC and SysEx → Verify last-write-wins

**Storage:**
1. Save patch → Power cycle → Load patch → Verify match
2. Save 128 patches → Verify no corruption
3. Corrupt patch → Verify error handling

### 7.3 Hardware Tests

**MIDI Controllers:**
- Korg nanoKONTROL2
- Behringer BCR2000
- Expression pedals (CC 1, 11)
- Footswitches (CC 64-67)

**Test Scenarios:**
1. Live parameter tweaking during performance
2. Bank select + program change for patch recall
3. Tap tempo via CC
4. Effect enable/disable via CC

## 8. Documentation Requirements

### 8.1 User Documentation

**CC Mapping Reference:**
- List of default CC assignments
- How to use slot selection mode
- How to use NRPN for full access
- Example MIDI controller setups

**Patch Management:**
- How to save/load patches
- Bank organization
- Naming conventions
- Backup/restore procedures

### 8.2 Developer Documentation

**Protocol Specification:**
- Complete CC mapping table
- NRPN addressing scheme
- Storage format specification
- Migration guide for existing patches

**API Reference:**
- Flash storage API
- CC mapping configuration API
- Patch import/export format

## 9. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|----------|
| Flash corruption | Low | High | CRC checks, backup SRAM |
| CC mapping confusion | Medium | Medium | Clear docs, sane defaults |
| SysEx/CC conflicts | Medium | Low | Last-write-wins, sync |
| Performance impact | Low | Medium | Lockless queues, profiling |
| Breaking changes | Low | High | Additive design, versioning |

## 10. Recommendations

### 10.1 Implementation Order

**Priority 1 (Must Have):**
1. Basic CC parameter control (Phase 1)
2. Program Change for patch recall
3. Simple patch storage (32 patches minimum)

**Priority 2 (Should Have):**
4. Bank Select for 128 patches
5. CC mapping configuration
6. Patch naming/organization

**Priority 3 (Nice to Have):**
7. NRPN for full parameter access
8. Slot selection mode
9. Takeover modes
10. Wear leveling

### 10.2 Architecture Decisions

**Keep SysEx Protocol:**
- ✅ Bidirectional sync with controller app
- ✅ Full metadata queries
- ✅ Status updates (CPU, levels)
- ✅ Proven stable system

**Add CC Protocol:**
- ✅ Standard MIDI controller support
- ✅ Real-time performance control
- ✅ Industry-standard approach
- ✅ No app dependency for basic control

**Design for Coexistence:**
- Both protocols active simultaneously
- Clear separation of concerns
- No breaking changes to existing SysEx
- Graceful degradation if CC disabled

### 10.3 Memory Allocation

**Recommended Flash Layout:**
```
0x90040000 - 0x906FFFFF: Firmware (6.75 MB) - current
0x90700000 - 0x9073FFFF: Patch Storage (256 KB) - new
  ├── 0x90700000: Header (4 KB)
  ├── 0x90701000: User Patches (128 KB, 256 patches)
  └── 0x90721000: Factory Patches (128 KB, backup)
0x90740000 - 0x907BFFFF: Reserved (512 KB) - future
```

**Backup SRAM (4 KB):**
```
0x38800000 - 0x388001FF: Current patch (512 bytes)
0x38800200 - 0x388009FF: Quick recall (4 patches × 512)
0x38800A00 - 0x38800FFF: Settings (1.5 KB)
```

## 11. Conclusion

The addition of standard MIDI CC support and patch storage to the DaisyMultiFX platform is **feasible and recommended**. The hybrid approach (SysEx + CC) provides the best of both worlds:

- **SysEx**: Comprehensive control, bidirectional sync, metadata
- **CC**: Real-time performance, standard controllers, no app required

**Key Benefits:**
1. ✅ Industry-standard MIDI controller compatibility
2. ✅ Performance-oriented real-time control
3. ✅ Persistent patch storage (128-256 patches)
4. ✅ No breaking changes to existing system
5. ✅ Clean separation of "programming" vs. "performance" modes

**Estimated Effort:**
- Phase 1 (CC Control): 2-3 days
- Phase 2 (Storage): 4-5 days
- Phase 3 (Advanced): 2-3 days
- **Total: 8-11 days** for full implementation

**Next Steps:**
1. Review and approve this design
2. Create detailed implementation tickets
3. Set up hardware test environment
4. Implement Phase 1 (CC control) as MVP
5. Iterate based on user feedback
