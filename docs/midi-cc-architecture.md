# MIDI CC Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DaisyMultiFX Pedal                                │
│                                                                           │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │                    USB MIDI Interface                           │    │
│  │  ┌───────────────┐              ┌────────────────┐             │    │
│  │  │  SysEx (F0...)│              │  CC (0xBn...)  │             │    │
│  │  │  ┌─────────┐  │              │  ┌──────────┐  │             │    │
│  │  │  │0x7D 0x20│  │              │  │0xB0 0x01 │  │             │    │
│  │  │  │SET_PARAM│  │              │  │  0x50    │  │             │    │
│  │  │  └─────────┘  │              │  └──────────┘  │             │    │
│  │  │               │              │                │             │    │
│  │  │  Programming  │              │  Performance   │             │    │
│  │  └───────┬───────┘              └────────┬───────┘             │    │
│  └──────────┼─────────────────────────────────┼───────────────────┘    │
│             │                                 │                         │
│             ▼                                 ▼                         │
│  ┌──────────────────────────────────────────────────────────────┐     │
│  │              MIDI Control Module                              │     │
│  │                                                                │     │
│  │  ┌──────────────────┐       ┌─────────────────────┐          │     │
│  │  │ SysEx Reassembly │       │  CC Message Parser  │          │     │
│  │  │  (ring buffer)   │       │  (instant decode)   │          │     │
│  │  └────────┬─────────┘       └──────────┬──────────┘          │     │
│  │           │                             │                     │     │
│  │           ▼                             ▼                     │     │
│  │  ┌────────────────────────────────────────────────┐          │     │
│  │  │       Command Dispatcher                        │          │     │
│  │  │  ┌──────────────┐      ┌──────────────┐        │          │     │
│  │  │  │ SysEx Queue  │      │  CC Queue    │        │          │     │
│  │  │  │ (4 messages) │      │ (lockless)   │        │          │     │
│  │  │  └──────┬───────┘      └──────┬───────┘        │          │     │
│  │  └─────────┼────────────────────┼─────────────────┘          │     │
│  └────────────┼────────────────────┼────────────────────────────┘     │
│               │                    │                                   │
│               ▼                    ▼                                   │
│  ┌────────────────────────────────────────────────────────────┐      │
│  │         Audio Callback (ISR, highest priority)              │      │
│  │                                                              │      │
│  │  1. Apply CC changes    (lowest latency)                    │      │
│  │  2. Apply SysEx changes (queued updates)                    │      │
│  │  3. Process audio blocks                                    │      │
│  │                                                              │      │
│  │  ┌────────────────────────────────────────────────┐         │      │
│  │  │      AudioProcessor & PedalBoard               │         │      │
│  │  │                                                 │         │      │
│  │  │  Slot 0 → Slot 1 → ... → Slot 11              │         │      │
│  │  │  [Delay]  [Reverb]        [Chorus]            │         │      │
│  │  └────────────────────────────────────────────────┘         │      │
│  └──────────────────────────────────────────────────────────────┘     │
│                                                                        │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │                Patch Storage (QSPI Flash)                    │     │
│  │                                                               │     │
│  │  0x90700000: ┌────────────────────────────────────┐          │     │
│  │              │  Header (4 KB)                     │          │     │
│  │              │  - Magic: 0xDA15YPAT               │          │     │
│  │              │  - Version, patch count            │          │     │
│  │              └────────────────────────────────────┘          │     │
│  │              ┌────────────────────────────────────┐          │     │
│  │              │  Bank 0 (16 patches, 8 KB)        │          │     │
│  │              │  Bank 1 (16 patches, 8 KB)        │          │     │
│  │              │  ...                               │          │     │
│  │              │  Bank 7 (16 patches, 8 KB)        │          │     │
│  │              └────────────────────────────────────┘          │     │
│  │              ┌────────────────────────────────────┐          │     │
│  │              │  Factory Patches (128 KB backup)  │          │     │
│  │              └────────────────────────────────────┘          │     │
│  │  0x90740000: Reserved (512 KB)                              │     │
│  └──────────────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────────────┘
```

## MIDI CC Mapping Modes

### Mode 1: Direct Mapping
```
┌──────────────────┐
│ MIDI Controller  │
│ (Korg, Behringer)│
└────────┬─────────┘
         │
         │ CC 1 = 80
         ▼
┌────────────────────┐
│  CC Mapping Table  │
│                    │
│  CC 1 → Slot 0     │
│         Param 0    │
└────────┬───────────┘
         │
         ▼
┌────────────────────┐
│   AudioProcessor   │
│   Slot 0, Param 0  │
│   Value = 80       │
└────────────────────┘
```

### Mode 2: Slot Selection
```
Step 1: Select Slot
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │
         │ CC 100 = 27  (scaled to slot 3)
         ▼
┌────────────────────┐
│   Active Slot = 3  │
└────────────────────┘

Step 2: Set Parameter
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │
         │ CC 101 = 64
         ▼
┌────────────────────┐
│  Slot 3, Param 0   │
│  Value = 64        │
└────────────────────┘
```

### Mode 3: NRPN
```
Step 1: Set NRPN MSB (Slot)
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │ CC 99 = 3
         ▼
┌────────────────────┐
│  NRPN_MSB = 3      │
└────────────────────┘

Step 2: Set NRPN LSB (Param)
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │ CC 98 = 2
         ▼
┌────────────────────┐
│  NRPN_LSB = 2      │
└────────────────────┘

Step 3: Set Value
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │ CC 6 = 80
         ▼
┌────────────────────┐
│  Slot 3, Param 2   │
│  Value = 80        │
└────────────────────┘
```

## Patch Recall Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    Patch Recall via MIDI                      │
└──────────────────────────────────────────────────────────────┘

Step 1: Bank Select
┌──────────────────┐
│ MIDI Controller  │
│  (Foot Switch)   │
└────────┬─────────┘
         │
         │ CC 0 = 3 (Bank 3)
         ▼
┌────────────────────┐
│  Current Bank = 3  │
└────────────────────┘

Step 2: Program Change
┌──────────────────┐
│ MIDI Controller  │
└────────┬─────────┘
         │
         │ Program Change = 7
         ▼
┌─────────────────────────────────┐
│  Calculate Patch Index:         │
│  Bank 3 × 16 + 7 = Patch 55     │
└────────┬────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  Read Flash:                    │
│  Address = 0x90700000           │
│          + 4KB (header)         │
│          + 55 × 512 bytes       │
│  = 0x90706E00                   │
└────────┬────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  Validate:                      │
│  - Check CRC32                  │
│  - Verify magic bytes           │
└────────┬────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  Load into Audio Processor:     │
│  - Parse 12 slots               │
│  - Set effect types             │
│  - Load parameters              │
│  - Configure routing            │
└─────────────────────────────────┘
```

## Coexistence: SysEx + CC

```
┌───────────────────────────────────────────────────────────────┐
│               Simultaneous Control Scenario                    │
└───────────────────────────────────────────────────────────────┘

Timeline:
  0ms                  10ms                 20ms
   │                    │                    │
   │  ┌────────────┐    │                    │
   │  │ Controller │    │                    │
   │  │   App      │    │                    │
   │  │  (SysEx)   │    │                    │
   │  └──────┬─────┘    │                    │
   │         │          │                    │
   │         │ F0 7D    │                    │
   │         │ 01 20    │                    │
   │         │ 00 02 50 │                    │
   │         │ F7       │                    │
   │         ▼          │                    │
   │  ┌──────────────┐  │                    │
   │  │  Slot 0,     │  │                    │
   │  │  Param 2     │  │                    │
   │  │  = 80        │  │                    │
   │  └──────────────┘  │                    │
   │                    │  ┌────────────┐    │
   │                    │  │   MIDI     │    │
   │                    │  │ Controller │    │
   │                    │  │   (CC)     │    │
   │                    │  └──────┬─────┘    │
   │                    │         │          │
   │                    │         │ 0xB0     │
   │                    │         │ 0x01     │
   │                    │         │ 0x64     │
   │                    │         ▼          │
   │                    │  ┌──────────────┐  │
   │                    │  │  Slot 0,     │  │
   │                    │  │  Param 0     │  │
   │                    │  │  = 100       │  │
   │                    │  └──────────────┘  │
   │                    │         │          │
   │                    │         │ Echo     │
   │                    │         ▼          │
   │                    │  ┌──────────────┐  │
   │  ◄─────────────────┼──┤  F0 7D 01    │  │
   │  Notification      │  │  20 00 00 64 │  │
   │  to App            │  │  F7          │  │
   │                    │  └──────────────┘  │
   │                    │                    │
   ▼                    ▼                    ▼

Result:
- Both changes applied successfully
- No conflicts (different parameters)
- App receives notification of CC change
- Everyone stays in sync
```

## Memory Layout

```
┌───────────────────────────────────────────────────────────────┐
│                     Daisy Seed Memory Map                      │
└───────────────────────────────────────────────────────────────┘

0x00000000  ┌─────────────────────────────────────┐
            │  ITCMRAM (64 KB)                    │  64 KB
            │  - Hot DSP code (zero wait state)   │
0x00010000  ├─────────────────────────────────────┤
            
0x20000000  ┌─────────────────────────────────────┐
            │  DTCMRAM (128 KB)                   │  128 KB
            │  - Stack, fast variables            │
0x20020000  ├─────────────────────────────────────┤

0x24000000  ┌─────────────────────────────────────┐
            │  SRAM (512 KB)                      │  512 KB
            │  - General heap                     │
0x24080000  ├─────────────────────────────────────┤

0x38800000  ┌─────────────────────────────────────┐
            │  BACKUP_SRAM (4 KB, battery)        │  4 KB
            │  ├─ Current patch (512 bytes)       │
            │  ├─ Quick recall (2048 bytes)       │
            │  └─ Settings (1536 bytes)           │
0x38801000  ├─────────────────────────────────────┤

0x90040000  ┌─────────────────────────────────────┐
            │  QSPI Flash (7936 KB)               │
            │                                      │
            │  Firmware & Data (6.75 MB)          │  6912 KB
            │  - Code, rodata, constants          │
            │  - Embedded models, IR data         │
0x90700000  ├─────────────────────────────────────┤
            │  Patch Storage (256 KB) ◄── NEW     │  256 KB
            │  ├─ Header (4 KB)                   │
            │  ├─ User Patches (128 KB)           │
            │  │   8 banks × 16 patches           │
            │  │   = 128 patches @ 512 bytes      │
            │  └─ Factory Patches (124 KB)        │
0x90740000  ├─────────────────────────────────────┤
            │  Reserved (512 KB)                  │  512 KB
0x907C0000  └─────────────────────────────────────┘

0xC0000000  ┌─────────────────────────────────────┐
            │  SDRAM (64 MB)                      │  65536 KB
            │  - Delay line buffers               │
            │  - Reverb working memory            │
            │  - Large effect buffers             │
0xC4000000  └─────────────────────────────────────┘

Total Used: ~75 MB
Available:  Ample headroom for expansion
```

## Implementation Phases Timeline

```
Phase 1: CC Support (2-3 days)
├─ Day 1: CC parser + direct mapping
├─ Day 2: Audio callback integration
└─ Day 3: Testing with hardware controller

Phase 2: Patch Storage (4-5 days)
├─ Day 1: Flash driver + storage layout
├─ Day 2: Save/load implementation
├─ Day 3: Program Change + Bank Select
├─ Day 4: Testing + CRC validation
└─ Day 5: Integration with CC system

Phase 3: Advanced (2-3 days)
├─ Day 1: Slot selection + NRPN
├─ Day 2: Takeover modes + CC mapping config
└─ Day 3: Polish + documentation

Total: 8-11 days
```

## Signal Flow Diagram

```
┌───────────────────────────────────────────────────────────────┐
│                    Audio Signal Flow                           │
└───────────────────────────────────────────────────────────────┘

Hardware Input
     │
     ▼
┌─────────────────┐
│  Codec (24-bit) │
│  48 kHz, 48 smp │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│  Input Gain (CC 11 or SysEx 0x27)       │
│  Range: 0 to +24 dB                     │
└────────┬────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│  Slot 0: Effect or Passthrough          │
│  - Type selected via CC 64 or SysEx 0x22│
│  - Parameters via CC or SysEx 0x20      │
│  - Enabled via CC or SysEx 0x21         │
└────────┬────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│  Slot 1...                               │
└────────┬────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│  Slot 11: Final effect                  │
└────────┬────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│  Output Gain (CC 7 or SysEx 0x28)       │
│  Range: -12 to +12 dB                   │
└────────┬────────────────────────────────┘
         │
         ▼
┌─────────────────┐
│  Codec Output   │
│  48 kHz, 48 smp │
└────────┬────────┘
         │
         ▼
  Hardware Output
```

---

**Note:** All diagrams are simplified representations. Actual implementation may vary based on performance requirements and hardware constraints.
