import React from "react";
import { View, ViewStyle, StyleProp } from "react-native";
import { useColors } from "../hooks/useColors";

interface CardProps {
  children: React.ReactNode;
  selected?: boolean;
  style?: StyleProp<ViewStyle>;
  flex?: number;
}

export const Card: React.FC<CardProps> = ({
  children,
  selected = false,
  style,
  flex,
}) => {
  const colors = useColors();
  return (
    <View
      style={[
        {
          backgroundColor: selected ? colors.primaryTint : colors.surface,
          padding: 16,
          borderRadius: 12,
          shadowColor: "#000",
          shadowOffset: { width: 0, height: 2 },
          shadowOpacity: 0.1,
          shadowRadius: 4,
          elevation: 3,
          borderWidth: 2,
          borderColor: selected ? colors.primary : colors.border,
        },
        style,
        flex !== undefined && { flex },
      ]}
    >
      {children}
    </View>
  );
};
