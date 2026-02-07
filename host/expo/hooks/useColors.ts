import { useColorScheme } from "react-native";

/**
 * Complete set of semantic color tokens used across the app.
 * Every component should pull colors from this palette
 * so that light/dark mode works automatically.
 */
export interface ColorTokens {
  // ── Surface / background ──
  /** Main screen background */
  background: string;
  /** Card / elevated surface background */
  surface: string;
  /** Slightly recessed surface (e.g. meter track, picker bg) */
  surfaceRecessed: string;

  // ── Text ──
  text: string;
  textSecondary: string;
  textTertiary: string;
  textDisabled: string;

  // ── Borders ──
  border: string;
  borderLight: string;
  separator: string;

  // ── Primary accent (blue) ──
  primary: string;
  /** Stronger/emphasized variant of primary (e.g. selected text, active borders) */
  primaryDark: string;
  /** Softer variant of primary (e.g. light borders, subtle highlights) */
  primaryLight: string;
  /** Very light tint of primary, used for fills / backgrounds */
  primaryTint: string;
  primaryOnTint: string;

  // ── States ──
  success: string;
  danger: string;
  dangerDark: string;
  warning: string;

  // ── Component-specific ──
  /** Footswitch outer border when disabled/empty */
  footswitchBorderDisabled: string;
  footswitchBorderEmpty: string;
  /** Footswitch inner border when disabled */
  footswitchInnerDisabled: string;
  /** Disabled strip / placeholder background */
  disabledBg: string;
  disabledBorder: string;
  /** Overlay scrim (e.g. modal backdrop) */
  overlay: string;
  /** Routing line color */
  routingLine: string;
}

const lightColors: ColorTokens = {
  // Surface
  background: "#f5f5f5",
  surface: "#fff",
  surfaceRecessed: "#f0f0f0",

  // Text
  text: "#333",
  textSecondary: "#666",
  textTertiary: "#888",
  textDisabled: "#999",

  // Borders
  border: "#EEEEEE",
  borderLight: "#E0E0E0",
  separator: "#eee",

  // Primary
  primary: "#2196F3",
  primaryDark: "#1976D2",
  primaryLight: "#90CAF9",
  primaryTint: "#E3F2FD",
  primaryOnTint: "#1976D2",

  // States
  success: "#4CAF50",
  danger: "#F44336",
  dangerDark: "#D32F2F",
  warning: "#FF9800",

  // Component-specific
  footswitchBorderDisabled: "#D0D0D0",
  footswitchBorderEmpty: "#E0E0E0",
  footswitchInnerDisabled: "#9E9E9E",
  disabledBg: "#E0E0E0",
  disabledBorder: "#D0D0D0",
  overlay: "rgba(0,0,0,0.5)",
  routingLine: "#1976D2",
};

const darkColors: ColorTokens = {
  // Surface
  background: "#121212",
  surface: "#1E1E1E",
  surfaceRecessed: "#2A2A2A",

  // Text
  text: "#E0E0E0",
  textSecondary: "#AAAAAA",
  textTertiary: "#888888",
  textDisabled: "#666666",

  // Borders
  border: "#333333",
  borderLight: "#444444",
  separator: "#333333",

  // Primary
  primary: "#42A5F5",
  primaryDark: "#64B5F6",
  primaryLight: "#1565C0",
  primaryTint: "#1A2F45",
  primaryOnTint: "#90CAF9",

  // States
  success: "#66BB6A",
  danger: "#EF5350",
  dangerDark: "#E53935",
  warning: "#FFA726",

  // Component-specific
  footswitchBorderDisabled: "#555555",
  footswitchBorderEmpty: "#444444",
  footswitchInnerDisabled: "#666666",
  disabledBg: "#333333",
  disabledBorder: "#444444",
  overlay: "rgba(0,0,0,0.7)",
  routingLine: "#64B5F6",
};

/**
 * Returns the full color palette for the current system color scheme.
 *
 * Usage:
 * ```ts
 * const colors = useColors();
 * <View style={{ backgroundColor: colors.surface }} />
 * ```
 */
export function useColors(): ColorTokens {
  const scheme = useColorScheme();
  return scheme === "dark" ? darkColors : lightColors;
}
