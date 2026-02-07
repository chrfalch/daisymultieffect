import { matchFont } from "@shopify/react-native-skia";

/** Pedal slot dimensions — kept in sync with routingGraph layout */
export const PEDAL_W = 90;
export const PEDAL_H = 120;
export const STRIP_H = 28;
export const CORNER_R = 8;
export const FOOTSWITCH_R = 19;
export const FOOTSWITCH_INNER_R = 9;
export const MAX_DISPLAY_TEXT_LENGTH = 12;
export const LONG_PRESS_DURATION_MS = 300;

// ─── Fonts ───────────────────────────────────────────────────────────
export const FONT_BOLD_18 = matchFont({
  fontFamily: "System",
  fontSize: 18,
  fontWeight: "bold",
});
export const FONT_NORMAL_10 = matchFont({
  fontFamily: "System",
  fontSize: 10,
});
export const FONT_NORMAL_8 = matchFont({
  fontFamily: "System",
  fontSize: 8,
});
export const FONT_BOLD_9 = matchFont({
  fontFamily: "System",
  fontSize: 9,
  fontWeight: "bold",
});

/** Measure text width using the skia font (approximate via char count when unavailable). */
export function textWidth(text: string, font: ReturnType<typeof matchFont>): number {
  // matchFont returns an SkFont with measureText
  if (font && typeof font.measureText === "function") {
    return font.measureText(text).width;
  }
  return text.length * 7; // rough fallback
}
