import React, { createContext, useMemo } from "react";
import { useColorScheme } from "react-native";

export interface ThemeColors {
  // Backgrounds
  background: string;
  surfacePrimary: string;
  surfaceSecondary: string;
  surfaceSelected: string;
  surfaceSelectedBorder: string;

  // Text
  text: string;
  textSecondary: string;
  textTertiary: string;
  textInverse: string;

  // Accent
  accent: string;
  accentLight: string;
  accentDark: string;

  // Status
  success: string;
  error: string;
  warning: string;

  // Borders
  border: string;
  borderLight: string;

  // Specific
  sliderFill: string;
  bypassActive: string;
  bypassActiveText: string;
  bypassInactive: string;
  bypassInactiveText: string;
}

const lightColors: ThemeColors = {
  background: "#f5f5f5",
  surfacePrimary: "#fff",
  surfaceSecondary: "#f0f0f0",
  surfaceSelected: "#E3F2FD",
  surfaceSelectedBorder: "#2196F3",

  text: "#333",
  textSecondary: "#666",
  textTertiary: "#999",
  textInverse: "#fff",

  accent: "#2196F3",
  accentLight: "#E3F2FD",
  accentDark: "#1976D2",

  success: "#4CAF50",
  error: "#F44336",
  warning: "#FF9800",

  border: "#EEEEEE",
  borderLight: "#eee",

  sliderFill: "#E3F2FD",
  bypassActive: "#F44336",
  bypassActiveText: "#fff",
  bypassInactive: "#E0E0E0",
  bypassInactiveText: "#666",
};

const darkColors: ThemeColors = {
  background: "#1a1a1a",
  surfacePrimary: "#2a2a2a",
  surfaceSecondary: "#333",
  surfaceSelected: "#1a3a5c",
  surfaceSelectedBorder: "#2196F3",

  text: "#f0f0f0",
  textSecondary: "#aaa",
  textTertiary: "#777",
  textInverse: "#1a1a1a",

  accent: "#42A5F5",
  accentLight: "#1a3a5c",
  accentDark: "#90CAF9",

  success: "#66BB6A",
  error: "#EF5350",
  warning: "#FFA726",

  border: "#3a3a3a",
  borderLight: "#444",

  sliderFill: "#1a3a5c",
  bypassActive: "#EF5350",
  bypassActiveText: "#fff",
  bypassInactive: "#3a3a3a",
  bypassInactiveText: "#aaa",
};

export interface Theme {
  colors: ThemeColors;
  isDark: boolean;
}

export const ThemeContext = createContext<Theme>({
  colors: lightColors,
  isDark: false,
});

export function useTheme(): Theme {
  return React.use(ThemeContext);
}

export function useThemeColors(): ThemeColors {
  return React.use(ThemeContext).colors;
}

export const ThemeProvider: React.FC<{ children: React.ReactNode }> = ({
  children,
}) => {
  const colorScheme = useColorScheme();
  const isDark = colorScheme === "dark";

  const theme = useMemo<Theme>(
    () => ({
      colors: isDark ? darkColors : lightColors,
      isDark,
    }),
    [isDark],
  );

  return <ThemeContext.Provider value={theme}>{children}</ThemeContext.Provider>;
};
