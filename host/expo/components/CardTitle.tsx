import React from "react";
import { Text, TextStyle, StyleProp } from "react-native";
import { useColors } from "../hooks/useColors";

interface CardTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const CardTitle: React.FC<CardTitleProps> = ({ children, style }) => {
  const colors = useColors();
  return (
    <Text
      style={[
        { fontSize: 18, fontWeight: "600", marginBottom: 12, color: colors.text },
        style,
      ]}
    >
      {children}
    </Text>
  );
};
