# Wah-Wah Pedal Effect

The Wah effect simulates the classic wah-wah pedal, made famous by guitarists like Jimi Hendrix and Eric Clapton. It creates the distinctive "wah" vocal-like sound by sweeping a resonant bandpass filter across the frequency spectrum.

**New Features:** Adjustable Q-factor for custom resonance and envelope follower for auto-wah functionality.

## Overview

The Wah effect uses a high-Q (high resonance) bandpass filter that emphasizes a narrow band of frequencies. As you sweep the position parameter, the filter's center frequency moves across the vocal formant range, creating the characteristic "wah" sound that mimics a human voice saying "wah."

The effect now supports two modes:
- **Manual Mode:** Traditional wah with user-controlled position (ideal for expression pedal)
- **Auto Mode:** Envelope follower that automatically controls position based on playing dynamics

## Parameters

### Position (0-100%)
Controls the center frequency of the bandpass filter (Manual mode) or starting position (Auto mode).

**Range:** 0% (low frequency, 400 Hz) to 100% (high frequency, 2000 Hz)  
**Default:** 50% (approximately 900 Hz)

**Tips:**
- **0-30%**: Deep, bass-heavy "wah" sound
- **30-70%**: Mid-range vocal formants (most expressive range)
- **70-100%**: Bright, treble-heavy "wah" sound
- Sweep the parameter slowly for classic wah effects
- Use quick movements for more aggressive, funky sounds
- In Auto mode, this parameter is overridden by the envelope follower

### Q Factor (4-20)
Controls the resonance (sharpness) of the filter peak.

**Range:** 4 (mild resonance) to 20 (extreme resonance)  
**Default:** 12 (classic wah sound)

**Tips:**
- **4-8**: Mild, smooth wah character - more subtle and musical
- **10-14**: Classic wah sound - balanced resonance (Cry Baby range)
- **15-20**: Extreme resonance - very pronounced, aggressive peak
- Lower Q values work well for funk and rhythm playing
- Higher Q values create more dramatic, cutting tones for leads
- Experiment to find your sweet spot - not all settings suit all playing styles

### Mode (Manual/Auto)
Selects between manual position control or automatic envelope-following.

**Options:** Manual, Auto  
**Default:** Manual

**Manual Mode:**
- Position parameter directly controls filter frequency
- Traditional wah pedal behavior
- Best for expression pedal control or preset static positions
- Full manual control over sweep

**Auto Mode:**
- Position is controlled by input signal envelope
- Creates touch-responsive, dynamic wah effect
- Filter "opens" (sweeps up) when you play harder
- Filter "closes" (sweeps down) when you play softer
- Great for auto-wah effects that respond to your playing dynamics

### Attack (1-100ms)
Controls how quickly the envelope follower responds to signal increases (Auto mode only).

**Range:** 1ms (instant) to 100ms (slow)  
**Default:** 10ms

**Tips:**
- **1-10ms**: Fast response - wah "opens" immediately when you pick
- **10-30ms**: Medium response - smooth, natural feel
- **30-100ms**: Slow response - more gradual, swelling effect
- Faster attack times are better for percussive playing
- Slower attack times create smoother, more vocal-like sweeps

### Release (10-500ms)
Controls how quickly the envelope follower responds to signal decreases (Auto mode only).

**Range:** 10ms (fast) to 500ms (slow)  
**Default:** 100ms

**Tips:**
- **10-50ms**: Fast decay - wah "closes" quickly after notes
- **50-200ms**: Medium decay - natural envelope following
- **200-500ms**: Slow decay - long sustaining wah effect
- Longer release times create more sustained, vocal-like sweeps
- Shorter release times are tighter and more rhythmic

### Sensitivity (0-100%)
Controls how much the input signal envelope affects the position (Auto mode only).

**Range:** 0% (no effect) to 100% (maximum effect)  
**Default:** 50%

**Tips:**
- **0-30%**: Subtle auto-wah - small position changes
- **30-70%**: Moderate response - balanced dynamic range
- **70-100%**: Extreme response - full filter sweep with dynamics
- Lower sensitivity for subtle, tight auto-wah
- Higher sensitivity for dramatic, wide-sweeping auto-wah
- Adjust based on your playing dynamics and pickup output

## How It Works

The wah effect uses a 2nd-order bandpass filter with:
- **Frequency Range:** 400 Hz to 2 kHz (classic wah vocal formant range)
- **Q Factor:** 4-20 (adjustable resonance)
- **Filter Type:** IIR Biquad bandpass (efficient, musical)
- **Frequency Mapping:** Exponential (provides more natural feel when sweeping)
- **Envelope Follower:** Peak detection with attack/release smoothing

The Q factor controls the resonance peak strength. Higher Q values create a stronger resonant peak that emphasizes harmonics in a narrow frequency band while attenuating other frequencies. This creates the distinctive vocal quality of the wah sound.

In Auto mode, the envelope follower tracks the input signal's amplitude envelope using peak detection with separate attack and release time constants. The envelope is then scaled by the sensitivity parameter and mapped to the position parameter, creating dynamic, touch-responsive wah effects.

## Usage Examples

### Classic Rhythm Wah (Funky) - Manual Mode
- **Mode:** Manual
- **Position:** Sweep rhythmically between 30% and 70%
- **Q Factor:** 12-15 (classic to aggressive)
- **Timing:** Sync with the beat (eighth notes or sixteenth notes)
- **Style:** Short, percussive strokes aligned with wah sweeps

### Lead Guitar Wah - Manual Mode
- **Mode:** Manual
- **Position:** Sweep slowly from 20% to 90% during sustained notes
- **Q Factor:** 10-12 (classic sound)
- **Technique:** Start low, sweep up during the note
- **Effect:** Adds vocal-like expression to leads

### Auto-Wah Funk - Auto Mode
- **Mode:** Auto
- **Q Factor:** 12-14 (classic funk sound)
- **Attack:** 5-10ms (fast response to pick attack)
- **Release:** 50-100ms (moderate decay)
- **Sensitivity:** 60-80% (strong dynamic response)
- **Style:** Play with dynamics - dig in for more wah, play lighter for less
- **Effect:** Filter opens with pick attack, closes during sustain

### Subtle Auto-Wah - Auto Mode
- **Mode:** Auto
- **Q Factor:** 6-8 (mild resonance)
- **Attack:** 20-30ms (smooth response)
- **Release:** 150-250ms (long sustain)
- **Sensitivity:** 30-50% (subtle effect)
- **Style:** Clean, fingerstyle playing
- **Effect:** Gentle, voice-like response to playing dynamics

### Static Wah (Fixed Position) - Manual Mode
- **Mode:** Manual
- **Position:** 60-70% (sweet spot)
- **Q Factor:** 8-10 (moderate resonance)
- **Use:** Leave position fixed for a resonant tone shaping effect
- **Benefit:** Adds presence and character without movement

### Bass Wah - Manual Mode
- **Mode:** Manual
- **Position:** 10-40% range
- **Q Factor:** 8-10 (moderate resonance)
- **Technique:** Subtle movements in the lower frequency range
- **Effect:** Adds definition and punch to bass lines without losing low end

## Expression Pedal (Manual Mode)

The Manual mode is designed with expression pedal control in mind. With an expression pedal (future hardware integration), the Position parameter can be controlled in real-time with your foot, allowing for classic wah pedal playing technique.

When expression pedal support is added:
- Heel down = 0% (low frequency, "woo" sound)
- Toe down = 100% (high frequency, "wah" sound)
- Full foot control over the sweep
- Can adjust Q Factor to customize resonance amount
- Works with any Q Factor setting for personalized wah character

## Technical Details

### Filter Implementation
- **Type:** 2nd-order IIR bandpass filter (Biquad)
- **Coefficients:** Based on RBJ Audio EQ Cookbook
- **State Variables:** Direct Form I (cache-friendly)
- **Stereo Processing:** Independent L/R filter states
- **Q Factor Range:** 4 to 20 (user-adjustable)

### Frequency Mapping
The effect uses exponential frequency mapping:
```
f(x) = 400 * (2000/400)^x = 400 * 5^x Hz
```
where x is the position parameter (0-1).

This exponential mapping provides:
- More control in the mid-range (where most expressiveness occurs)
- Natural feel when sweeping the parameter
- Better distribution across the hearing range

### Q Factor Mapping
```
Q = 4 + qFactor * 16
```
where qFactor is the normalized parameter (0-1).

This provides:
- **Q=4**: Mild, smooth resonance (qFactor=0%)
- **Q=12**: Classic wah resonance (qFactor=50%)
- **Q=20**: Extreme, aggressive resonance (qFactor=100%)

### Envelope Follower
The auto-wah uses a peak detection envelope follower:
- **Algorithm:** Exponential attack/release smoothing
- **Attack coefficient:** `exp(-1 / (attackTime * sampleRate))`
- **Release coefficient:** `exp(-1 / (releaseTime * sampleRate))`
- **Sensitivity scaling:** Envelope × Sensitivity × 10 (for typical guitar levels)
- **Position update:** Only updates filter when position changes > 0.1% (optimization)

### CPU Usage
The enhanced wah effect remains very efficient:
- **Manual mode:** ~2-3% CPU on Daisy Seed @ 48kHz (single instance)
- **Auto mode:** ~3-4% CPU on Daisy Seed @ 48kHz (includes envelope follower)
- **Maximum instances:** 4 per patch
- **Per-sample cost:** ~150-200 cycles (2 biquad filters + envelope in auto mode)

## Tips and Tricks

### Manual Mode Tips

1. **Find the Sweet Spot:** The most expressive range is typically 40-70%. Spend more time in this range when sweeping.

2. **Heel Down = Low, Toe Down = High:** If you're familiar with real wah pedals, remember that heel down (0%) is the low "woo" sound, and toe down (100%) is the high "wah" sound.

3. **Rhythm is Key:** For funky rhythm wah, timing the sweeps with your picking is more important than the exact position.

4. **Don't Oversweep:** You don't need to sweep the full 0-100% range every time. Small, controlled movements in the 30-70% range often sound more musical.

5. **Try Fixed Positions:** Setting the wah to a fixed position can be useful as a tone-shaping EQ. Try 65% for added presence on rhythm parts.

6. **Experiment with Q Factor:** Different Q values change the wah character dramatically. Try lower Q (6-8) for smooth, subtle wah, or higher Q (15-18) for aggressive, cutting tones.

### Auto Mode Tips

1. **Start with Medium Settings:** Begin with Attack=10ms, Release=100ms, Sensitivity=50%, then adjust to taste.

2. **Match Attack to Playing Style:** Fast attack (5-10ms) for percussive funk, slower attack (20-50ms) for smoother, vocal-like sweeps.

3. **Release for Sustain:** Longer release times (200-400ms) create more sustained wah sweeps that follow your note decay.

4. **Sensitivity for Dynamics:** Lower sensitivity (30-50%) for subtle effect, higher sensitivity (70-90%) for dramatic auto-wah.

5. **Playing Dynamics Matter:** Auto-wah responds to how hard you play. Dig in for more sweep, play lightly for less effect.

6. **Combine with Q Factor:** Lower Q (6-8) with auto-wah creates smooth, filter-like sweeps. Higher Q (12-16) creates more pronounced, vocal-like auto-wah.

7. **Envelope Tracking:** The envelope follower tracks the peak amplitude. Single notes work best - complex chords can create unpredictable sweeps.

### General Tips

8. **Combine with Overdrive:** Place an overdrive effect before the wah for classic rock tones. The distortion adds harmonics that the wah emphasizes beautifully.

9. **Adjust Playing Technique:** With wah engaged, cleaner picking and muting techniques often sound better than when playing without wah.

10. **Mode Switching:** Switch between Manual and Auto modes during a song for different feels - manual for controlled passages, auto for dynamic, responsive sections.

## Combining with Other Effects

### Before Wah:
- **Compressor:** Evens out dynamics for consistent wah response
- **Overdrive/Distortion:** Classic setup for rock/blues (generates harmonics for wah to emphasize)
- **Octave:** Creates unique, synth-like tones

### After Wah:
- **Chorus/Flanger:** Adds movement and depth to wah sweeps
- **Delay:** Creates rhythmic echoes of wah patterns
- **Reverb:** Adds space and ambience

### Avoid:
- **EQ before wah:** Can reduce effectiveness by pre-filtering frequencies
- **Heavy effects after wah:** Can muddy the clear vocal character

## Known Limitations

## Known Limitations

- Expression pedal hardware support not yet implemented (manual position control via MIDI)
- Fixed frequency range (400 Hz - 2 kHz) - future versions may offer alternative ranges
- Auto mode envelope follower works best with single-note lines (complex chords may cause erratic behavior)
- No wet/dry mix parameter (100% wet signal)

## History

The wah-wah pedal was invented in 1966 by Brad Plunkett at Warwick Electronics (Thomas Organ Company). Initially designed to make a guitar sound like a trumpet, it was first used extensively by guitarists like Clyde McCoy. The effect became famous in rock music through artists like:

- **Jimi Hendrix:** "Voodoo Child (Slight Return)" - iconic wah-soaked lead tones
- **Eric Clapton (Cream):** "White Room" - rhythmic wah accompaniment
- **Isaac Hayes:** "Theme from Shaft" - classic funk auto-wah sound
- **Metallica:** "Enter Sandman" (intro) - heavy metal wah riff
- **Stevie Ray Vaughan:** Extensive use throughout his career

The most famous wah pedals include:
- **Vox V846:** Original 1960s design with aggressive character
- **Dunlop Cry Baby:** Industry standard since 1970s, warm vintage tone
- **Morley:** Known for optical/switchless design, smooth sweep

This digital implementation captures the essential frequency sweep and resonant character of these classic pedals, with the added benefits of:
- **Adjustable Q-factor** for customizing resonance (not possible on most analog pedals)
- **Auto-wah mode** with independent attack/release/sensitivity controls
- **Preset recall** via MIDI for instant settings changes
- **No moving parts** (unlike mechanical pedals that wear out)

## Future Enhancements

Planned improvements for the wah effect:
1. **Expression Pedal Input:** Hardware integration for real-time foot control in Manual mode
2. **Frequency Range Options:** Alternative sweep ranges (bass wah, treble wah presets)
3. **Mix Parameter:** Wet/dry blend control for parallel processing
4. **Multiple Filter Modes:** Bandpass (current), lowpass, highpass options
5. **LFO Mode:** Automatic sweep with adjustable rate (like auto-wah but tempo-based)
