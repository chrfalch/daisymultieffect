import React from "react";
import { Text, StyleSheet, TextStyle, StyleProp } from "react-native";

interface SectionTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const SectionTitle: React.FC<SectionTitleProps> = ({
  children,
  style,
}) => {
  return <Text style={[styles.title, style]}>{children}</Text>;
};

const styles = StyleSheet.create({
  title: {
    fontSize: 14,
    fontWeight: "600",
    color: "#666",
    marginBottom: 8,
    textTransform: "uppercase",
    letterSpacing: 0.5,
  },
});
