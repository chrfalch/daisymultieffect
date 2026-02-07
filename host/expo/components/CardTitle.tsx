import React from "react";
import { Text, StyleSheet, TextStyle, StyleProp } from "react-native";
import { useThemeColors } from "./ThemeProvider";

interface CardTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const CardTitle: React.FC<CardTitleProps> = ({ children, style }) => {
  const colors = useThemeColors();
  return (
    <Text style={[styles.title, { color: colors.text }, style]}>
      {children}
    </Text>
  );
};

const styles = StyleSheet.create({
  title: {
    fontSize: 18,
    fontWeight: "600",
    marginBottom: 12,
  },
});
