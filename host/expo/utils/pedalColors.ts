/**
 * Color mapping for pedal effect types.
 *
 * Each effect type is assigned a color based on its "category".  Since
 * EffectMeta doesn't carry an explicit category field we derive it from
 * the well-known typeId values defined in the firmware.
 */

export interface PedalColorTheme {
  /** Accent color used for the top strip background */
  strip: string;
  /** Lighter tint used for the pedal body background */
  body: string;
  /** Text color on the top strip */
  stripText: string;
}

// Category palettes
const PALETTES = {
  bypass: { strip: "#9E9E9E", body: "#F5F5F5", stripText: "#fff" },
  distortion: { strip: "#E65100", body: "#FFF3E0", stripText: "#fff" },
  dynamics: { strip: "#0277BD", body: "#E1F5FE", stripText: "#fff" },
  modulation: { strip: "#7B1FA2", body: "#F3E5F5", stripText: "#fff" },
  timeBased: { strip: "#00796B", body: "#E0F2F1", stripText: "#fff" },
  eq: { strip: "#2E7D32", body: "#E8F5E9", stripText: "#fff" },
  pitch: { strip: "#F9A825", body: "#FFFDE7", stripText: "#333" },
  utility: { strip: "#546E7A", body: "#ECEFF1", stripText: "#fff" },
  amp: { strip: "#C62828", body: "#FFEBEE", stripText: "#fff" },
} as const satisfies Record<string, PedalColorTheme>;

/**
 * Map from effect typeId → colour theme.
 * Unknown types fall back to utility grey.
 */
const TYPE_COLOR_MAP: Record<number, PedalColorTheme> = {
  0: PALETTES.bypass, // Off
  1: PALETTES.timeBased, // Delay
  10: PALETTES.distortion, // Overdrive
  12: PALETTES.timeBased, // Sweep Delay
  13: PALETTES.utility, // Mixer
  14: PALETTES.timeBased, // Reverb
  15: PALETTES.dynamics, // Compressor
  16: PALETTES.modulation, // Chorus
  17: PALETTES.dynamics, // Noise Gate
  18: PALETTES.eq, // Graphic EQ
  19: PALETTES.modulation, // Flanger
  20: PALETTES.modulation, // Phaser
  21: PALETTES.amp, // Neural Amp
  22: PALETTES.amp, // Cabinet IR
  23: PALETTES.modulation, // Tremolo
  24: PALETTES.utility, // Tuner
  25: PALETTES.pitch, // Pitch Shifter
  26: PALETTES.timeBased, // Shimmer Reverb
  27: PALETTES.modulation, // Leslie
};

export function getColorForType(typeId: number): PedalColorTheme {
  return TYPE_COLOR_MAP[typeId] ?? PALETTES.utility;
}
