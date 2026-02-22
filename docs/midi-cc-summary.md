# MIDI Control Change Support - Executive Summary

## Quick Overview

This document summarizes the analysis for adding standard MIDI Control Change (CC) support and patch storage to the DaisyMultiFX pedal. For full details, see [midi-cc-analysis.md](./midi-cc-analysis.md).

## Current State

**What Works Today:**
- ✅ Full parameter control via custom SysEx messages (manufacturer ID 0x7D)
- ✅ 12 effect slots with up to 8 parameters each
- ✅ Controller app (iOS/iPad) sends patches via MIDI SysEx
- ✅ Bidirectional sync: firmware sends state updates back to app
- ✅ All messages use 7-bit safe encoding (MIDI compatible)

**What's Missing:**
- ❌ No support for standard MIDI CC messages
- ❌ Cannot use generic MIDI controllers (e.g., Korg, Behringer, etc.)
- ❌ No persistent patch storage (patches lost on power cycle)
- ❌ Must always use controller app for parameter changes

## Proposed Solution: Hybrid Architecture

### Keep SysEx Protocol (for Programming)
- Controller app continues to use SysEx for full control
- Bidirectional sync, metadata queries, status updates
- No breaking changes to existing system

### Add CC Protocol (for Performance)
- Standard MIDI controllers can adjust parameters in real-time
- Bank Select + Program Change for patch recall
- No app required for basic performance use

## MIDI CC Mapping Strategy

### Approach 1: Direct Mapping (Simplest)
```
CC 1   → Slot 0, Param 0 (e.g., Delay Time)
CC 2   → Slot 0, Param 1 (e.g., Delay Feedback)
CC 7   → Output Gain (global)
CC 11  → Input Gain (global)
CC 64  → Slot 0 Enable/Disable
CC 65  → Slot 1 Enable/Disable
...
```

**Pros:** Simple, immediate
**Cons:** Only ~120 CCs available, can't reach all 96 parameters

### Approach 2: Slot Selection Mode (Flexible)
```
CC 100     → Select Active Slot (0-11)
CC 101-108 → Control Parameters 0-7 of active slot
```

**Example Workflow:**
1. Send CC 100 = 27 (select slot 3, scaled from 0-127 → 0-11)
2. Send CC 101 = 64 (set parameter 0 of slot 3 to 64)
3. Send CC 102 = 100 (set parameter 1 of slot 3 to 100)

**Pros:** Access all 96 parameters with ~16 CCs
**Cons:** Two-step process (select slot, then parameter)

### Approach 3: NRPN (Industry Standard)
```
CC 99  → NRPN MSB (slot number 0-11)
CC 98  → NRPN LSB (parameter ID 0-7)
CC 6   → Data Entry MSB (value 0-127)
```

**Example:**
1. CC 99 = 3 (select slot 3)
2. CC 98 = 2 (select parameter 2)
3. CC 6 = 80 (set value to 80)

**Pros:** Industry standard, full parameter access
**Cons:** Three-step process

### Recommended: Hybrid of All Three
- **Direct mapping** for frequently-used parameters (CC 1-20)
- **Slot selection** for quick access during performance (CC 100-108)
- **NRPN** for complete access when needed (CC 98-99, 6)

## Patch Storage Design

### Memory Available
- **QSPI Flash:** 7.9 MB total (currently used for firmware)
- **Proposal:** Allocate 256 KB for patch storage
- **Capacity:** 256-512 patches (depends on patch size)

### Storage Architecture
```
QSPI Flash Layout:
├── 0x90040000: Firmware code/data (6.75 MB) - existing
├── 0x90700000: Patch Storage (256 KB) - new
│   ├── Header (4 KB): Magic, version, patch count
│   ├── User Patches (128 KB): 256 patches @ 512 bytes each
│   └── Factory Patches (124 KB): Backup/restore
└── 0x90740000: Reserved (512 KB) - future expansion

Backup SRAM (4 KB, battery-backed):
├── Current active patch (512 bytes)
├── Quick recall buffer (4 recent patches)
└── Settings/calibration (remaining space)
```

### Patch Organization
- **8 banks × 16 patches = 128 patches** (starter config)
- Can expand to 256 patches if needed
- Each patch: ~360 bytes actual data + metadata
- Bank names (16 chars), Patch names (24 chars)
- CRC32 for integrity checking

### Patch Recall via MIDI
```
CC 0 (Bank Select MSB) = 3  → Select Bank 3
Program Change = 7           → Load Patch 7 from Bank 3
Result: Patch 3:7 loaded (index 55)
```

## Coexistence: SysEx + CC

### Message Priority (in audio callback)
1. **CC messages** - Applied first (lowest latency for performance)
2. **SysEx messages** - Applied second (programming/configuration)
3. **Audio processing** - DSP runs with updated parameters

### Conflict Resolution
**When CC and SysEx change same parameter:**
- **Last-Write-Wins:** Most recent change takes precedence
- **Bidirectional Sync:** CC changes trigger SysEx notifications to app
- **No Echo:** SysEx changes don't echo back as CC (prevents loops)

**Example Flow:**
1. User tweaks knob on MIDI controller → CC 1 = 80
2. Firmware applies parameter change
3. Firmware sends SysEx notification to app
4. App updates UI to show new value
5. No conflict, everyone in sync

## Implementation Phases

### Phase 1: Basic CC Support (2-3 days)
**Goal:** Real-time parameter control via MIDI CC

- Add CC message parser to MIDI receive path
- Implement direct mapping (CC → slot/param lookup)
- Add CC handler to audio callback
- Test with Korg nanoKONTROL2

**Deliverable:** Can control pedal with standard MIDI controller

### Phase 2: Patch Storage (4-5 days)
**Goal:** Persistent patch storage in flash

- Implement flash driver for QSPI storage
- Create storage layout (header, banks, patches)
- Add SysEx commands: SAVE_PATCH, LOAD_PATCH, LIST_PATCHES
- Implement Program Change handler for patch recall
- Add Bank Select support (CC 0)

**Deliverable:** 128 patches stored/recalled via MIDI

### Phase 3: Advanced Features (2-3 days)
**Goal:** Full flexibility and polish

- Slot selection mode (CC 100-108)
- NRPN support (CC 98-99, 6)
- Takeover modes (pickup, immediate, scale)
- CC mapping configuration via SysEx
- Wear leveling for flash longevity

**Deliverable:** Production-ready CC system

**Total Effort: 8-11 days**

## Key Benefits

### For Users
1. ✅ Use any MIDI controller (no app required for basic control)
2. ✅ Real-time parameter tweaking during performance
3. ✅ Store up to 128-256 patches on device
4. ✅ Bank Select + Program Change for instant recall
5. ✅ Controller app still works exactly as before

### For Developers
1. ✅ Industry-standard MIDI protocol (CC, NRPN)
2. ✅ No breaking changes to existing SysEx
3. ✅ Clean separation: CC = performance, SysEx = programming
4. ✅ Additive design (can be disabled if needed)
5. ✅ Graceful coexistence of both protocols

## Potential Issues & Solutions

### Issue 1: 96 Parameters > 128 CC Numbers
**Solution:** Hybrid mapping (direct + slot selection + NRPN)

### Issue 2: Flash Wear (10,000 write cycles)
**Solution:** 
- Wear leveling algorithm
- Cache patches in RAM
- Save on manual trigger only (not auto-save)
- Expected lifetime: 10,000 / 10 patches per day = 2.7 years

### Issue 3: CC and SysEx Conflicts
**Solution:**
- Last-write-wins with timestamps
- Bidirectional notifications
- Clear documentation of behavior

### Issue 4: Patch Recall Latency
**Solution:**
- Preload next patch during idle
- Keep 4 recent patches in backup SRAM (instant recall)
- Crossfade patch switching (already planned)

## Test Hardware Recommendations

**MIDI Controllers to Test:**
- ✅ Korg nanoKONTROL2 (8 faders, 8 knobs, 24 buttons)
- ✅ Behringer BCR2000 (32 encoders, 20 buttons)
- ✅ Expression pedals (CC 1, 11)
- ✅ Footswitches (CC 64-67)

**Test Scenarios:**
1. Live parameter tweaking during audio playback
2. Bank + Program Change for patch recall
3. Interleaved CC and SysEx messages
4. Power cycle and patch recall
5. 100+ rapid parameter changes (stress test)

## Recommendations

### Priority 1: Must Have (MVP)
1. ✅ Basic CC parameter control (direct mapping)
2. ✅ Program Change for patch recall
3. ✅ Simple patch storage (32 patches minimum)

### Priority 2: Should Have
4. ✅ Bank Select (128 patches)
5. ✅ CC mapping configuration
6. ✅ Patch naming and organization

### Priority 3: Nice to Have
7. ⭐ NRPN for full parameter access
8. ⭐ Slot selection mode
9. ⭐ Takeover modes
10. ⭐ Wear leveling

## Decision: Proceed or Not?

### Arguments FOR Implementation
1. **Industry Standard:** Every professional effects processor supports MIDI CC
2. **No App Dependency:** Use pedal standalone with any MIDI controller
3. **Performance Oriented:** Real-time control without latency
4. **Additive:** No breaking changes, SysEx continues to work
5. **Memory Available:** Only 256 KB needed from 7.9 MB flash

### Arguments AGAINST Implementation
1. ~~Complexity~~ - Actually quite straightforward
2. ~~Breaking Changes~~ - None, it's additive
3. ~~Memory Constraints~~ - 256 KB is negligible (3% of flash)
4. ~~Conflicts with SysEx~~ - Solved with last-write-wins

### Recommendation: **PROCEED** ✅

The benefits far outweigh the costs. This transforms the pedal from a "controller-app-dependent device" to a **truly standalone professional effect processor** that happens to also have an optional app for advanced programming.

## Next Steps

1. **Review this analysis** with stakeholders
2. **Approve design** (or request changes)
3. **Create implementation tickets** (3 phases)
4. **Set up test environment** (MIDI controller, DAW, hardware pedal)
5. **Implement Phase 1** (CC support MVP) - 2-3 days
6. **User testing** and feedback
7. **Iterate** on Phases 2 and 3

## Questions?

For full technical details, see:
- [midi-cc-analysis.md](./midi-cc-analysis.md) - Complete 21 KB analysis
- [architecture.md](../architecture.md) - System architecture overview
- [README.md](../README.md) - Project overview

---

**Document Version:** 1.0
**Date:** 2024-02-06
**Author:** Copilot Analysis
**Status:** Ready for Review
