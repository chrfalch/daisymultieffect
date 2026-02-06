# Wah-Wah Pedal Effect

The Wah effect simulates the classic wah-wah pedal, made famous by guitarists like Jimi Hendrix and Eric Clapton. It creates the distinctive "wah" vocal-like sound by sweeping a resonant bandpass filter across the frequency spectrum.

## Overview

The Wah effect uses a high-Q (high resonance) bandpass filter that emphasizes a narrow band of frequencies. As you sweep the position parameter, the filter's center frequency moves across the vocal formant range, creating the characteristic "wah" sound that mimics a human voice saying "wah."

## Parameters

### Position (0-100%)
Controls the center frequency of the bandpass filter.

**Range:** 0% (low frequency, 400 Hz) to 100% (high frequency, 2000 Hz)  
**Default:** 50% (approximately 900 Hz)

**Tips:**
- **0-30%**: Deep, bass-heavy "wah" sound
- **30-70%**: Mid-range vocal formants (most expressive range)
- **70-100%**: Bright, treble-heavy "wah" sound
- Sweep the parameter slowly for classic wah effects
- Use quick movements for more aggressive, funky sounds

## How It Works

The wah effect uses a 2nd-order bandpass filter with:
- **Frequency Range:** 400 Hz to 2 kHz (classic wah vocal formant range)
- **Q Factor:** 12 (high resonance for characteristic wah sound)
- **Filter Type:** IIR Biquad bandpass (efficient, musical)
- **Frequency Mapping:** Exponential (provides more natural feel when sweeping)

The high Q factor (12) creates a strong resonant peak at the center frequency, which emphasizes harmonics in that frequency band while attenuating other frequencies. This creates the distinctive vocal quality of the wah sound.

## Usage Examples

### Classic Rhythm Wah (Funky)
- **Position:** Sweep rhythmically between 30% and 70%
- **Timing:** Sync with the beat (eighth notes or sixteenth notes)
- **Style:** Short, percussive strokes aligned with wah sweeps

### Lead Guitar Wah
- **Position:** Sweep slowly from 20% to 90% during sustained notes
- **Technique:** Start low, sweep up during the note
- **Effect:** Adds vocal-like expression to leads

### Static Wah (Fixed Position)
- **Position:** 60-70% (sweet spot)
- **Use:** Leave position fixed for a resonant tone shaping effect
- **Benefit:** Adds presence and character without movement

### Bass Wah
- **Position:** 10-40% range
- **Technique:** Subtle movements in the lower frequency range
- **Effect:** Adds definition and punch to bass lines without losing low end

## Expression Pedal (Future)

This implementation is designed with expression pedal control in mind. In a future update, the Position parameter will be controllable via an expression pedal, allowing real-time, foot-controlled sweeping of the wah effect.

When expression pedal support is added:
- Heel down = 0% (low frequency)
- Toe down = 100% (high frequency)
- Full foot control over the sweep

## Technical Details

### Filter Implementation
- **Type:** 2nd-order IIR bandpass filter (Biquad)
- **Coefficients:** Based on RBJ Audio EQ Cookbook
- **State Variables:** Direct Form I (cache-friendly)
- **Stereo Processing:** Independent L/R filter states

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

### CPU Usage
The wah effect is very efficient:
- **Typical usage:** ~2-3% CPU on Daisy Seed @ 48kHz (single instance)
- **Maximum instances:** 4 per patch
- **Per-sample cost:** ~100-150 cycles (2 biquad filters)

## Tips and Tricks

1. **Find the Sweet Spot:** The most expressive range is typically 40-70%. Spend more time in this range when sweeping.

2. **Heel Down = Low, Toe Down = High:** If you're familiar with real wah pedals, remember that heel down (0%) is the low "woo" sound, and toe down (100%) is the high "wah" sound.

3. **Rhythm is Key:** For funky rhythm wah, timing the sweeps with your picking is more important than the exact position.

4. **Don't Oversweep:** You don't need to sweep the full 0-100% range every time. Small, controlled movements in the 30-70% range often sound more musical.

5. **Try Fixed Positions:** Setting the wah to a fixed position can be useful as a tone-shaping EQ. Try 65% for added presence on rhythm parts.

6. **Combine with Overdrive:** Place an overdrive effect before the wah for classic rock tones. The distortion adds harmonics that the wah emphasizes beautifully.

7. **Adjust Playing Technique:** With wah engaged, cleaner picking and muting techniques often sound better than when playing without wah.

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

- Single parameter control (expression pedal support planned for future)
- Fixed Q factor (resonance amount is not adjustable)
- Fixed frequency range (400 Hz - 2 kHz)
- No auto-wah/envelope follower mode (may be added as separate effect)

## History

The wah-wah pedal was invented in 1966 by Brad Plunkett at Warwick Electronics (Thomas Organ Company). Initially designed to make a guitar sound like a trumpet, it was first used extensively by guitarists like Clyde McCoy. The effect became famous in rock music through artists like:

- **Jimi Hendrix:** "Voodoo Child (Slight Return)"
- **Eric Clapton (Cream):** "White Room"
- **Isaac Hayes:** "Theme from Shaft"
- **Metallica:** "Enter Sandman" (intro)

The most famous wah pedals include:
- **Vox V846:** Original 1960s design
- **Dunlop Cry Baby:** Industry standard since 1970s
- **Morley:** Known for optical/switchless design

This digital implementation captures the essential frequency sweep and resonant character of these classic pedals.

## Future Enhancements

Planned improvements for the wah effect:
1. **Expression Pedal Input:** Real-time foot control
2. **Auto-Wah Mode:** Envelope-following automatic sweep
3. **Q Factor Control:** Adjustable resonance amount
4. **Frequency Range Options:** Multiple sweep range presets
5. **Mix Parameter:** Wet/dry blend control
