import React from "react";
import { Text, StyleSheet, TextStyle, StyleProp } from "react-native";
import { useThemeColors } from "./ThemeProvider";

interface SectionTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const SectionTitle: React.FC<SectionTitleProps> = ({
  children,
  style,
}) => {
  const colors = useThemeColors();
  return (
    <Text style={[styles.title, { color: colors.textSecondary }, style]}>
      {children}
    </Text>
  );
};

const styles = StyleSheet.create({
  title: {
    fontSize: 14,
    fontWeight: "600",
    marginBottom: 8,
    textTransform: "uppercase",
    letterSpacing: 0.5,
  },
});
