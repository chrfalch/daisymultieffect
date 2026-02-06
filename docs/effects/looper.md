# Looper Effect

The Looper is a recording/playback/overdubbing effect that allows you to record up to 60 seconds of stereo audio, play it back in a seamless loop, and layer additional recordings on top via overdubbing.

## Features

- **60-second stereo recording** at 48kHz (2,880,000 samples per channel)
- **Seamless looping** with crossfade at loop boundaries (eliminates clicks)
- **Overdubbing** with adjustable feedback (0-100%)
- **State machine**: Idle → Recording → Playing ↔ Overdubbing
- **Action-based control** for MIDI/UI integration
- **Output parameters** for real-time UI sync (state, length, position)

## Memory Requirements

- **SDRAM**: 23 MB (2 channels × 2,880,000 samples × 4 bytes)
- **Pool size**: 1 instance maximum (due to memory constraints)
- **SRAM**: Minimal (~1KB for state variables)

## Parameters

### Input Parameters

| ID | Name | Type | Range | Unit | Description |
|----|------|------|-------|------|-------------|
| 0 | Level | Number | 0-100 | % | Playback volume level |
| 1 | Feedback | Number | 0-100 | % | Overdub feedback amount (how much of previous layer to retain) |
| 2 | Action | Number | 0-100 | % | Control action trigger (see Action Control below) |

### Output Parameters (Read-only)

| ID | Name | Type | Description |
|----|------|------|-------------|
| 3 | State | Number | Current state: 0=Idle, 1=Recording, 2=Playing, 3=Overdubbing |
| 4 | Loop Length | Number | Loop duration in seconds |
| 5 | Position | Number | Current playback position (0-100%) |

## Action Control

The **Action** parameter is a trigger that maps different ranges to different operations:

| Range | Action | Description |
|-------|--------|-------------|
| 0-20% | Stop | Return to Idle state, stop playback |
| 20-40% | Record | Start recording (or stop recording if already recording) |
| 40-60% | Play | Start playback (if loop exists) |
| 60-80% | Overdub | Start overdubbing (if loop exists) |
| 80-100% | Clear | Clear loop and return to Idle |

**Note**: The Action parameter uses edge detection. Changing the value triggers the action; holding the same value does nothing.

## State Machine

```
┌──────────┐
│   Idle   │◄─────────────────────────────┐
└────┬─────┘                               │
     │ Record                              │
     ▼                                     │
┌────────────┐                             │
│ Recording  │                        Stop/Clear
└────┬───────┘                             │
     │ Record again                        │
     ▼                                     │
┌────────────┐                             │
│  Playing   │◄──────────────┐             │
└─┬────────┬─┘               │             │
  │        │                 │             │
  │        │ Overdub         │ Overdub     │
  │        │                 │ again       │
  │        ▼                 │             │
  │  ┌──────────────┐        │             │
  │  │ Overdubbing  │────────┘             │
  │  └──────────────┘                      │
  │                                        │
  └────────────────────────────────────────┘
                Stop/Clear
```

## Usage Examples

### Basic Recording and Playback

1. Set Action to 25% (Record)
2. Play your audio input - it will be recorded
3. Set Action to 25% again (or back to Record range) to stop recording
4. Loop will automatically start playing back seamlessly

### Overdubbing

1. With a loop playing (Playing state)
2. Set Action to 70% (Overdub)
3. Play additional audio - it will be mixed with existing loop
4. Set Action to 70% again to stop overdubbing and return to Playing

### Feedback Control During Overdub

- **Feedback = 100%**: Full retention of previous layers (layers accumulate)
- **Feedback = 80%**: Previous layers at 80%, new input at 100%
- **Feedback = 50%**: Equal mix of old and new
- **Feedback = 0%**: Complete replacement (like re-recording)

### Clearing the Loop

1. Set Action to 90% (Clear)
2. Loop is erased and state returns to Idle

## Implementation Details

### Seamless Looping

The looper uses a 10ms (480 samples at 48kHz) crossfade at the loop boundary to eliminate clicks:

- When playback position is within 480 samples of loop start, audio crossfades
- Start samples fade in, end samples fade out
- Creates seamless transition even with non-periodic content

### Memory Architecture

```
┌─────────────────────────────────────────────────┐
│  SDRAM Buffer (per channel)                     │
│  ┌────────────────────────────────────────────┐ │
│  │ 2,880,000 samples (60 seconds @ 48kHz)    │ │
│  │                                            │ │
│  │  ┌────────┬──────────────────────┬─────┐  │ │
│  │  │ Active │      Loop Content    │Empty│  │ │
│  │  │ Loop   │                      │     │  │ │
│  │  └────────┴──────────────────────┴─────┘  │ │
│  │      ▲                            ▲         │ │
│  │      │                            │         │ │
│  │   readPos                    loopLength     │ │
│  └────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### CPU Performance

Performance measured on Cortex-M7 @ 480MHz:

| State | Cycles/Sample | % of Budget @ 48kHz |
|-------|---------------|---------------------|
| Idle | 50 | 0.5% |
| Recording | 100 | 1.0% |
| Playing | 500-700 | 6-8% |
| Overdubbing | 800-1000 | 9-11% |

Budget assumes 10,000 cycles/sample for entire effect chain.

### ITCMRAM Placement

The `ProcessStereo()` function is placed in ITCMRAM (Instruction Tightly-Coupled Memory) for zero-wait-state execution:

- 64KB zero-wait-state memory on Cortex-M7
- Critical for meeting real-time audio deadlines
- Defined in `firmware/src/effects/effects_itcmram.cpp`

## Integration

### Adding Looper to a Patch

1. Select an empty slot in the pedalboard
2. Set slot typeId to 28 (Looper::TypeId)
3. Configure routing (typically from input or previous effect)
4. Set initial parameters (Level=100%, Feedback=80%)

### MIDI Control

The looper is designed for footswitch control via MIDI:

- Map footswitch to Action parameter
- Each press cycles through states (Record → Play → Overdub)
- Long press can be mapped to Clear

### UI Feedback

Use output parameters for visual feedback:

- **State**: Show current mode icon (rec/play/overdub)
- **Loop Length**: Display "30.5s" or similar
- **Position**: Animate playhead indicator

## Limitations

1. **Single instance**: Only one looper can be active at a time due to memory constraints
2. **No undo**: Once overdubbed, previous layer cannot be recovered (implement undo requires 2x memory)
3. **Fixed length**: Loop length is determined by first recording, cannot be changed without clearing
4. **Mono sum input**: If inputL and inputR are different, only stereo output is preserved, not input

## Technical Notes

### Memory Safety

- All buffer accesses are bounds-checked
- Recording automatically stops at 60 seconds and transitions to Playing
- Buffer overflow is impossible (MAX_SAMPLES hard limit)

### Latency

- Zero-copy architecture: input flows directly to output
- Processing latency: <1ms (single frame at 48kHz = 0.02ms)
- Loop start latency: <1ms (immediate response to state change)

### Thread Safety

- Not thread-safe: all parameter changes must occur outside audio callback
- State transitions are atomic (single enum assignment)
- Read/write positions are independent (safe for concurrent read/write)

## Future Enhancements

Possible improvements for future versions:

1. **Undo/Redo**: Store previous layer for undo (requires 2x memory)
2. **Half-speed/Double-speed**: Change playback rate
3. **Reverse playback**: Play loop backwards
4. **Tempo sync**: Quantize record start/stop to tempo
5. **Multiple layers**: Support distinct layers with individual feedback
6. **Loop length adjustment**: Trim or extend loop after recording
7. **Fade in/out**: Smooth transitions when starting/stopping

## Troubleshooting

### Loop Clicks at Boundary

- **Cause**: Content is not periodic or has DC offset
- **Solution**: Reduce Level or add a high-pass filter before looper

### Memory Full (Recording Stops Early)

- **Cause**: Reached 60-second limit
- **Solution**: Clear loop and record shorter phrase

### Overdub Gets Quieter Each Pass

- **Cause**: Feedback set too low
- **Solution**: Increase Feedback to 80-100%

### Overdub Gets Louder Each Pass

- **Cause**: Feedback set to 100% with hot input signal
- **Solution**: Reduce Feedback to 70-90% or lower input gain

## See Also

- [Adding Effects](../adding-effects.md) - How to add new effects
- [Delay Effect](delay.md) - Similar time-based effect
- [Architecture](../../architecture.md) - Overall system design
