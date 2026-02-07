import React from "react";
import { View, StyleSheet, ViewStyle, StyleProp } from "react-native";
import { useThemeColors } from "./ThemeProvider";

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
  const colors = useThemeColors();

  return (
    <View
      style={[
        styles.card,
        {
          backgroundColor: colors.surfacePrimary,
          borderColor: colors.border,
        },
        selected && {
          backgroundColor: colors.surfaceSelected,
          borderColor: colors.surfaceSelectedBorder,
        },
        style,
        flex !== undefined && { flex },
      ]}
    >
      {children}
    </View>
  );
};

const styles = StyleSheet.create({
  card: {
    padding: 16,
    borderRadius: 12,
    borderCurve: "continuous",
    boxShadow: "0 2px 4px rgba(0, 0, 0, 0.1)",
    borderWidth: 2,
  },
});
