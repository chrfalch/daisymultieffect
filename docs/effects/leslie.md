# Leslie Rotating Speaker Effect

The Leslie effect simulates the classic Leslie rotating speaker cabinet, originally designed for Hammond organs. It creates a rich, swirling modulation effect by simulating separate rotating horn and bass speakers with Doppler pitch shifting, amplitude modulation, and stereo imaging.

## Overview

The Leslie effect splits your audio into two frequency bands (horn and bass) and processes each through a simulated rotating speaker. As the virtual speakers spin, they create:

- **Doppler Effect**: Pitch shifts as the speaker moves toward/away from the listener
- **Amplitude Modulation**: Volume changes based on speaker direction
- **Stereo Movement**: Spatial positioning creates a wide, immersive sound
- **Frequency Separation**: Crossover filter splits highs (horn) and lows (bass)

## Parameters

### Speed (Enum: Stop/Slow/Fast)
Controls the rotation speed mode matching Leslie 122 specifications:
- **Stop**: Rotors stop (with smooth deceleration)
- **Slow**: Classic slow rotation (Horn: 42 RPM, Bass: 30 RPM)
- **Fast**: Fast "tremolo" rotation (Horn: 400 RPM, Bass: 324 RPM)

**Tips:**
- Use **Slow** for classic organ sounds with gentle movement
- Use **Fast** for more dramatic tremolo and vibrato effects
- Switch between modes during performance for expressive dynamics

### Acceleration (0.1-5.0 seconds)
Time it takes for rotors to reach target speed after changing modes.

**Range:** 0.1s (instant) to 5.0s (gradual)  
**Default:** 1.0s

**Tips:**
- Shorter times (0.5-1.0s) for responsive, dramatic transitions
- Longer times (2.0-5.0s) for smooth, gradual speed changes
- Very short times can sound unnatural; 0.5-1.5s is typical

### Separation (0-100%)
Controls stereo width of the effect.

**Range:** 0% (mono) to 100% (full stereo)  
**Default:** 50%

**Tips:**
- 100% for maximum spatial movement and stereo spread
- 50% for balanced stereo imaging
- Lower values for more centered, less dramatic stereo effect
- 0% for mono output (useful for bass-heavy material)

### Horn Level (0-100%)
Mix level of the high-frequency (horn) rotor.

**Range:** 0% (muted) to 100% (full level)  
**Default:** 70%

**Tips:**
- Balance with Bass Level to shape tonal character
- Reduce for darker, bass-heavy sounds
- Increase for brighter, more present highs
- 70% provides good balance for most sources

### Bass Level (0-100%)
Mix level of the low-frequency (bass) rotor.

**Range:** 0% (muted) to 100% (full level)  
**Default:** 70%

**Tips:**
- Balance with Horn Level for desired tone
- Increase for fuller, deeper sound
- Reduce for thinner, more horn-focused effect
- 70% provides good balance for most sources

### Crossover (400-1200 Hz)
Frequency where audio is split between horn (highs) and bass (lows).

**Range:** 400 Hz to 1200 Hz  
**Default:** 800 Hz

**Tips:**
- **400-600 Hz**: More bass content in bass rotor, thinner horn
- **800 Hz**: Classic Leslie split point (default)
- **1000-1200 Hz**: More mids in horn rotor, tighter bass
- Higher crossover = brighter overall tone
- Lower crossover = warmer overall tone

### Drive (0-100%)
Input saturation/distortion applied before the effect.

**Range:** 0% (clean) to 100% (heavily saturated)  
**Default:** 0%

**Tips:**
- 0% for clean, pristine Leslie effect
- 10-30% for subtle warmth and harmonic content
- 50%+ for overdriven organ-style distortion
- Works well with organ sounds and electric piano

### Mix (0-100%)
Wet/dry blend between processed and original signal.

**Range:** 0% (dry) to 100% (wet)  
**Default:** 100%

**Tips:**
- 100% for full Leslie effect (typical for organs)
- 50-70% for subtle movement on guitars/keys
- 30-50% for adding character without overwhelming the signal
- Parallel mixing (lower values) preserves punch and clarity

## Usage Examples

### Classic Hammond Organ
- **Speed:** Slow/Fast (switch during performance)
- **Acceleration:** 1.0-2.0s
- **Separation:** 80-100%
- **Horn/Bass Levels:** 70%/70%
- **Crossover:** 800 Hz
- **Drive:** 0-20%
- **Mix:** 100%

### Electric Piano
- **Speed:** Slow
- **Acceleration:** 2.0-3.0s
- **Separation:** 60%
- **Horn/Bass Levels:** 60%/70%
- **Crossover:** 600 Hz
- **Drive:** 0%
- **Mix:** 60-80%

### Guitar Chorus/Vibrato
- **Speed:** Slow or Fast
- **Acceleration:** 1.0s
- **Separation:** 50-70%
- **Horn/Bass Levels:** 50%/50%
- **Crossover:** 1000 Hz
- **Drive:** 0-10%
- **Mix:** 40-60%

### Bass Enhancement
- **Speed:** Slow
- **Acceleration:** 2.0s
- **Separation:** 30-50%
- **Horn/Bass Levels:** 30%/80%
- **Crossover:** 400 Hz
- **Drive:** 0%
- **Mix:** 30-50%

## Technical Details

### Physical Modeling
The Leslie effect uses physical modeling to simulate:
- **Horn radius:** 0.19 m (7.5 inches)
- **Bass radius:** 0.24 m (9.4 inches)  
- **Speed of sound:** 343 m/s (used for Doppler calculations)

### Crossover Filter
4th-order Butterworth crossover (two cascaded 2nd-order biquads):
- **Horn:** 4th-order highpass
- **Bass:** 4th-order lowpass
- **Slope:** 24 dB/octave (very sharp separation)

### CPU Usage
The Leslie effect is moderately CPU-intensive due to:
- 8 biquad filters (crossover)
- 4 delay buffers with interpolation (Doppler)
- Trigonometric calculations (rotation)

**Typical usage:** ~20-25% CPU on Daisy Seed @ 48kHz (single instance)  
**Maximum instances:** 2 per patch

## Tips and Tricks

1. **Speed Transitions:** Use the Speed parameter for dramatic performance dynamics. Fast-to-slow transitions are particularly expressive.

2. **Stereo Width:** Experiment with Separation. Full stereo (100%) is dramatic but can be too much for some mixes. 50-70% often sounds more musical.

3. **Drive Amount:** Even small amounts of drive (5-15%) add warmth and character without obvious distortion.

4. **Mix for Subtlety:** Lower Mix values (40-70%) let you add Leslie movement without overwhelming your original tone.

5. **Crossover Tuning:** Adjust the crossover frequency based on your instrument:
   - **Organ:** 800 Hz (classic)
   - **Electric Piano:** 600-700 Hz
   - **Guitar:** 1000-1200 Hz

6. **Horn/Bass Balance:** These controls shape the overall tone. Start at 70%/70%, then adjust:
   - More horn = brighter, more present
   - More bass = fuller, warmer

7. **Acceleration Feel:** Longer acceleration times (2-4s) feel more natural and organic. Shorter times (0.5-1s) are more immediate and dramatic.

## Combining with Other Effects

### Before Leslie:
- **Overdrive/Distortion:** Classic organ sound
- **Compressor:** Evens out dynamics before rotation
- **EQ:** Shape tone before splitting into horn/bass

### After Leslie:
- **Reverb:** Adds space and ambience
- **Delay:** Creates rhythmic echoes with movement
- **Cabinet IR:** Simulates speaker cabinet coloration

## Known Limitations

- Maximum 2 instances per patch (CPU intensive)
- Mono input is duplicated to stereo (no true mono mode)
- No acceleration curve adjustment (exponential only)
- Fixed horn/bass rotor radius and speed ratios

## History

The Leslie speaker was invented by Don Leslie in 1941 and became synonymous with the Hammond organ sound. The rotating speaker cabinet used separate treble horn and bass rotor, each spinning at different speeds, creating the iconic swirling sound heard in jazz, rock, and gospel music.

This digital simulation captures the essential characteristics of the Leslie 122 and 147 models, the most popular Leslie cabinets.
