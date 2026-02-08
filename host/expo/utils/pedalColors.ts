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

// Category palettes — light mode
const LIGHT_PALETTES = {
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

// Category palettes — dark mode (deeper body tints, brighter strips)
const DARK_PALETTES = {
  bypass: { strip: "#757575", body: "#333", stripText: "#fff" },
  distortion: { strip: "#FF6D00", body: "#3E2723", stripText: "#fff" },
  dynamics: { strip: "#039BE5", body: "#1A237E", stripText: "#fff" },
  modulation: { strip: "#AB47BC", body: "#311B92", stripText: "#fff" },
  timeBased: { strip: "#26A69A", body: "#004D40", stripText: "#fff" },
  eq: { strip: "#43A047", body: "#1B5E20", stripText: "#fff" },
  pitch: { strip: "#FFD54F", body: "#4A3F00", stripText: "#333" },
  utility: { strip: "#78909C", body: "#37474F", stripText: "#fff" },
  amp: { strip: "#EF5350", body: "#4A1010", stripText: "#fff" },
} as const satisfies Record<string, PedalColorTheme>;

type Category = keyof typeof LIGHT_PALETTES;

/**
 * Map from effect typeId → category name.
 * Unknown types fall back to utility.
 */
const TYPE_CATEGORY_MAP: Record<number, Category> = {
  0: "bypass",       // Off
  1: "timeBased",    // Delay
  10: "distortion",  // Overdrive
  12: "timeBased",   // Sweep Delay
  13: "utility",     // Mixer
  14: "timeBased",   // Reverb
  15: "dynamics",    // Compressor
  16: "modulation",  // Chorus
  17: "dynamics",    // Noise Gate
  18: "eq",          // Graphic EQ
  19: "modulation",  // Flanger
  20: "modulation",  // Phaser
  21: "amp",         // Neural Amp
  22: "amp",         // Cabinet IR
  23: "modulation",  // Tremolo
  24: "utility",     // Tuner
  25: "pitch",       // Pitch Shifter
  26: "timeBased",   // Shimmer Reverb
  27: "modulation",  // Leslie
};

export function getColorForType(typeId: number, isDark = false): PedalColorTheme {
  const category = TYPE_CATEGORY_MAP[typeId] ?? "utility";
  return isDark ? DARK_PALETTES[category] : LIGHT_PALETTES[category];
}
