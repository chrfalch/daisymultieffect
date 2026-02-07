import React from "react";
import { Text, TextStyle, StyleProp } from "react-native";
import { useColors } from "../hooks/useColors";

interface SectionTitleProps {
  children: React.ReactNode;
  style?: StyleProp<TextStyle>;
}

export const SectionTitle: React.FC<SectionTitleProps> = ({
  children,
  style,
}) => {
  const colors = useColors();
  return (
    <Text
      style={[
        {
          fontSize: 14,
          fontWeight: "600",
          color: colors.textSecondary,
          marginBottom: 8,
          textTransform: "uppercase",
          letterSpacing: 0.5,
        },
        style,
      ]}
    >
      {children}
    </Text>
  );
};
