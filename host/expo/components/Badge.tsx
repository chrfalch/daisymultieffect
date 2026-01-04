import React from "react";
import {
  Pressable,
  StyleProp,
  StyleSheet,
  Text,
  TextStyle,
  ViewStyle,
} from "react-native";

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
  return (
    <Pressable
      onPress={onPress}
      disabled={!onPress}
      style={({ pressed }) => [
        styles.badge,
        selected && styles.badgeSelected,
        pressed && styles.badgePressed,
        style,
      ]}
    >
      <Text
        style={[
          styles.badgeText,
          selected && styles.badgeTextSelected,
          textStyle,
        ]}
      >
        {label}
      </Text>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  badge: {
    backgroundColor: "#E3F2FD",
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
  },
  badgeSelected: {
    backgroundColor: "#2196F3",
  },
  badgePressed: {
    opacity: 0.7,
  },
  badgeText: {
    color: "#1976D2",
    fontSize: 14,
  },
  badgeTextSelected: {
    color: "#fff",
    fontWeight: "600",
  },
});
