# Looper Quick Reference

## Type ID
**28** (Effects::Looper::TypeId)

## Parameters

| ID | Name | Range | Default | Description |
|----|------|-------|---------|-------------|
| 0 | Level | 0-100% | 100% | Playback volume |
| 1 | Feedback | 0-100% | 80% | Overdub feedback amount |
| 2 | Action | 0-100% | 0% | Control trigger (see Action Map) |

## Output Parameters (Read-only)

| ID | Name | Type | Description |
|----|------|------|-------------|
| 3 | State | 0-3 | 0=Idle, 1=Recording, 2=Playing, 3=Overdubbing |
| 4 | Loop Length | float | Duration in seconds |
| 5 | Position | 0-1 | Playback position (normalized) |

## Action Map

| Value | Range | Action | Description |
|-------|-------|--------|-------------|
| 0-19% | 0.0-0.19 | **Stop** | Stop playback, return to Idle |
| 20-39% | 0.2-0.39 | **Record** | Start/stop recording |
| 40-59% | 0.4-0.59 | **Play** | Start playback |
| 60-79% | 0.6-0.79 | **Overdub** | Start/stop overdubbing |
| 80-100% | 0.8-1.0 | **Clear** | Clear loop, return to Idle |

## MIDI CC Values (0-127)

| Action | CC Value | Calculation |
|--------|----------|-------------|
| Stop | 0-24 | 0-19% × 127 |
| Record | 25-49 | 20-39% × 127 |
| Play | 50-74 | 40-59% × 127 |
| Overdub | 75-99 | 60-79% × 127 |
| Clear | 100-127 | 80-100% × 127 |

## State Machine

```
Idle --[Record]--> Recording --[Record]--> Playing
                                             |  ^
                                  [Overdub]  |  |  [Overdub]
                                             v  |
                                          Overdubbing
```

Any state → Idle: Set Action to Stop (0-19%) or Clear (80-100%)

## Memory Requirements

- **Buffer size**: 11.5 MB SDRAM (30 seconds stereo @ 48kHz)
- **Max instances**: 1
- **Pool constant**: `kMaxLoopers = 1`

## Code Snippets

### Initialize
```cpp
LooperEffect looper;
float bufL[LooperEffect::MAX_SAMPLES];
float bufR[LooperEffect::MAX_SAMPLES];
looper.BindBuffers(bufL, bufR);
looper.Init(48000.0f);
```

### Set Parameters
```cpp
looper.SetParam(0, 1.0f);   // Level = 100%
looper.SetParam(1, 0.8f);   // Feedback = 80%
looper.SetParam(2, 0.3f);   // Action = Record (30%)
```

### Process Audio
```cpp
float l = inputL;
float r = inputR;
looper.ProcessStereo(l, r);
outputL = l;
outputR = r;
```

### Read State
```cpp
OutputParamDesc outputs[3];
uint8_t count = looper.GetOutputParams(outputs, 3);
int state = (int)outputs[0].value;
float length = outputs[1].value;
float position = outputs[2].value;
```

## Integration Checklist

- [ ] Include `effects/looper.h`
- [ ] Add to patch with TypeId 28
- [ ] Allocate SDRAM buffers (or use BindProcessorBuffers)
- [ ] Call BindBuffers() before Init()
- [ ] Map footswitch/MIDI to Action parameter
- [ ] Display output parameters in UI
- [ ] Test record → play → overdub workflow

## Common Actions

| Task | Action Value | Notes |
|------|--------------|-------|
| Start recording | 0.3 (30%) | From Idle state |
| Stop recording | 0.3 (30%) | From Recording state → Playing |
| Start playback | 0.5 (50%) | If loop exists |
| Start overdub | 0.7 (70%) | From Playing state |
| Stop overdub | 0.7 (70%) | From Overdubbing → Playing |
| Clear loop | 0.9 (90%) | From any state → Idle |

## Metadata Access
```cpp
const EffectMeta& meta = Effects::Looper::kMeta;
// meta.name = "Looper"
// meta.shortName = "LPR"
// meta.description = "Record, playback, and overdub loops up to 30 seconds."
// meta.numParams = 6
```

## CPU Budget (Cortex-M7 @ 480MHz)

| State | Cycles/Sample | % Budget |
|-------|---------------|----------|
| Idle | 50 | 0.5% |
| Recording | 100 | 1% |
| Playing | 600 | 6% |
| Overdubbing | 900 | 9% |

Assumes 10,000 cycles/sample total budget.

## Feedback Guidelines

| Feedback | Behavior | Use Case |
|----------|----------|----------|
| 0-30% | Replacement | Re-recording |
| 40-60% | Equal mix | Building layers |
| 70-90% | Layer retention | Accumulating parts |
| 95-100% | Full retention | Infinite sustain |

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Clicks at loop boundary | Non-periodic content | Reduce Level parameter |
| Overdub too loud | Feedback too high | Reduce Feedback to 70-80% |
| Overdub too quiet | Feedback too low | Increase Feedback to 85-95% |
| Recording stops early | 30s limit reached | Clear and record shorter |
| No playback | Loop not recorded | Record first, then play |

## See Also

- [Looper Documentation](looper.md) - Complete documentation
- [Looper Examples](looper-examples.md) - Usage examples
- [Adding Effects](../adding-effects.md) - Effect architecture
