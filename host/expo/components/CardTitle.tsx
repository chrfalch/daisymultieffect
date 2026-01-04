import React from "react";
import { Text, StyleSheet, TextStyle, StyleProp } from "react-native";

interface CardTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const CardTitle: React.FC<CardTitleProps> = ({ children, style }) => {
  return <Text style={[styles.title, style]}>{children}</Text>;
};

const styles = StyleSheet.create({
  title: {
    fontSize: 18,
    fontWeight: "600",
    marginBottom: 12,
  },
});
