import React from "react";
import {
  Pressable,
  Text,
  ViewStyle,
  StyleProp,
} from "react-native";
import { useColors } from "../hooks/useColors";

interface ButtonProps {
  title: string;
  onPress: () => void;
  style?: StyleProp<ViewStyle>;
}

export const Button: React.FC<ButtonProps> = ({ title, onPress, style }) => {
  const colors = useColors();
  return (
    <Pressable
      style={({ pressed }) => [
        {
          backgroundColor: colors.primary,
          paddingHorizontal: 16,
          paddingVertical: 8,
          borderRadius: 8,
        },
        pressed && { opacity: 0.7 },
        style,
      ]}
      onPress={onPress}
    >
      <Text style={{ color: "#fff", fontWeight: "500" }}>{title}</Text>
    </Pressable>
  );
};
