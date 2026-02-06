# Looper Configuration Guide

This guide explains how to configure the looper effect's maximum loop duration.

## Changing Loop Duration

The looper's maximum loop time can be easily adjusted by modifying a single constant in the code.

### Location

**File**: `core/effects/looper.h`

**Constant**: `LOOPER_DURATION_SECONDS`

### How to Change

1. Open `core/effects/looper.h`
2. Find the configuration section (around line 16-25)
3. Change the value of `LOOPER_DURATION_SECONDS`
4. Rebuild the firmware or VST

```cpp
// Example: Change from 30 seconds to 60 seconds
static constexpr int LOOPER_DURATION_SECONDS = 60;  // was 30
```

### Memory Requirements

The looper uses SDRAM for audio storage. The memory required is calculated as:

```
Memory (bytes) = LOOPER_DURATION_SECONDS × 48,000 samples/s × 2 channels × 4 bytes/sample
Memory (MB) = LOOPER_DURATION_SECONDS × 0.384
```

#### Memory Table

| Duration | Samples/Channel | Memory Required | % of 64MB SDRAM |
|----------|----------------|-----------------|-----------------|
| 10s | 480,000 | ~3.8 MB | 6% |
| 15s | 720,000 | ~5.8 MB | 9% |
| 20s | 960,000 | ~7.7 MB | 12% |
| **30s** | **1,440,000** | **~11.5 MB** | **18%** (default) |
| 45s | 2,160,000 | ~17.3 MB | 27% |
| 60s | 2,880,000 | ~23 MB | 36% |
| 90s | 4,320,000 | ~34.6 MB | 54% |
| 120s | 5,760,000 | ~46 MB | 72% |

### Considerations

#### Memory Constraints
- **Total SDRAM**: The Daisy Seed has 64 MB of SDRAM
- **Other effects**: Consider memory used by other effects (delays, reverbs, etc.)
- **Practical limit**: Leaving some headroom, 90-120 seconds is likely the practical maximum if the looper is the only large-buffer effect

#### CPU Performance
- CPU usage is independent of buffer size
- Recording: ~100 cycles/sample
- Playing: ~600 cycles/sample
- Overdubbing: ~900 cycles/sample

#### Use Cases

**Short durations (10-20s)**
- ✅ Minimal memory footprint
- ✅ More memory for other effects
- ✅ Good for rhythmic loops and short phrases
- ⚠️ May be limiting for longer compositions

**Medium durations (30-45s)**
- ✅ Balanced memory usage
- ✅ Handles most musical phrases
- ✅ Room for other effects
- ✅ Default recommendation

**Long durations (60-120s)**
- ✅ Extended composition capabilities
- ✅ Full song sections
- ⚠️ Significant memory usage
- ⚠️ May limit other large-buffer effects

### Example Configurations

#### Minimal Memory (10 seconds)
```cpp
static constexpr int LOOPER_DURATION_SECONDS = 10;
```
Use when: Conserving memory for many other effects

#### Balanced (30 seconds - Default)
```cpp
static constexpr int LOOPER_DURATION_SECONDS = 30;
```
Use when: General-purpose looping with good memory balance

#### Extended (60 seconds)
```cpp
static constexpr int LOOPER_DURATION_SECONDS = 60;
```
Use when: Longer compositions, fewer concurrent effects

#### Maximum Practical (90-120 seconds)
```cpp
static constexpr int LOOPER_DURATION_SECONDS = 90;  // or 120
```
Use when: Looper is primary/only large-buffer effect

### Build Process

After changing the duration:

#### VST Plugin
```bash
cd vst/build
cmake --build . --config Release
```

#### Firmware
```bash
cd firmware
make clean
make -j4
make flash
```

### Verification

After rebuilding, you can verify the change:

1. The effect metadata will still show the old duration in descriptions (this is for documentation clarity)
2. The actual buffer size will match your new setting
3. Test by recording up to your new maximum duration

### Automatic Duration Updates

Note that the following are **not** automatically updated when you change `LOOPER_DURATION_SECONDS`:

- Effect metadata description (shows "30 seconds" regardless)
- Documentation files
- Comments in other files

These remain at their documented values for consistency. The actual buffer size and functionality will match your configured duration.

### Troubleshooting

#### "Insufficient memory" errors
- Reduce `LOOPER_DURATION_SECONDS`
- Reduce the number of other effects in your patch
- Check memory usage of other large-buffer effects

#### Firmware won't flash
- Verify the new duration doesn't exceed available SDRAM
- Try a smaller duration
- Check that other effects aren't using excessive memory

#### Recording stops before expected duration
- Verify the change was applied correctly
- Rebuild with `make clean` before `make`
- Check that MAX_SAMPLES is calculated correctly in looper.h

## Technical Details

### Implementation

The duration is implemented as a compile-time constant:

```cpp
static constexpr int LOOPER_DURATION_SECONDS = 30;
static constexpr int MAX_SAMPLES = 48000 * LOOPER_DURATION_SECONDS;
```

This means:
- Zero runtime overhead (calculated at compile time)
- Type-safe (compiler catches invalid values)
- Optimal code generation

### Buffer Allocation

Buffers are allocated in SDRAM (external memory):

```cpp
DSY_SDRAM_BSS float g_looperBufL[kMaxLoopers][LooperEffect::MAX_SAMPLES];
DSY_SDRAM_BSS float g_looperBufR[kMaxLoopers][LooperEffect::MAX_SAMPLES];
```

The `DSY_SDRAM_BSS` macro places the buffers in external SDRAM, not internal SRAM.

### Sample Rate

The looper is fixed at 48 kHz sample rate. To change the sample rate, you would need to modify:
- The `48000` constant in MAX_SAMPLES calculation
- The audio hardware configuration
- All time-based effects

This is not recommended unless you're making platform-wide changes.

## See Also

- [Looper Effect Documentation](looper.md) - Complete effect documentation
- [Looper Examples](looper-examples.md) - Usage examples
- [Looper Quick Reference](looper-quick-ref.md) - Parameter reference
- [Adding Effects](../adding-effects.md) - How effects are integrated
