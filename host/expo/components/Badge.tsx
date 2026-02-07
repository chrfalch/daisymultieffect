import React from "react";
import {
  Pressable,
  StyleProp,
  Text,
  TextStyle,
  ViewStyle,
} from "react-native";
import { useColors } from "../hooks/useColors";

export interface BadgeProps {
  label: string;
  selected?: boolean;
  onPress?: () => void;
  style?: StyleProp<ViewStyle>;
  textStyle?: StyleProp<TextStyle>;
}

export const Badge: React.FC<BadgeProps> = ({
  label,
  selected = false,
  onPress,
  style,
  textStyle,
}) => {
  const colors = useColors();
  return (
    <Pressable
      onPress={onPress}
      disabled={!onPress}
      style={({ pressed }) => [
        {
          backgroundColor: selected ? colors.primary : colors.primaryTint,
          paddingHorizontal: 12,
          paddingVertical: 6,
          borderRadius: 16,
        },
        pressed && { opacity: 0.7 },
        style,
      ]}
    >
      <Text
        style={[
          {
            color: selected ? "#fff" : colors.primaryOnTint,
            fontSize: 14,
            fontWeight: selected ? "600" : "400",
          },
          textStyle,
        ]}
      >
        {label}
      </Text>
    </Pressable>
  );
};
