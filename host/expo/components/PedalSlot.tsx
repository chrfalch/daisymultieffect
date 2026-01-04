import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Card } from "./Card";

interface PedalSlotProps {
  name: string;
  enabled: boolean;
  selected?: boolean;
  onPress: () => void;
  onToggleEnabled: () => void;
}

export const PedalSlot: React.FC<PedalSlotProps> = ({
  name,
  enabled,
  selected = false,
  onPress,
  onToggleEnabled,
}) => {
  return (
    <Pressable
      style={styles.container}
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={`Select ${name} details`}
    >
      <Card selected={selected}>
        <View style={styles.header}>
          <Text style={[styles.title, selected && styles.titleSelected]}>
            {name}
          </Text>
          <Pressable onPress={onToggleEnabled}>
            {({ pressed }) => (
              <View
                style={[
                  styles.enabledBadge,
                  {
                    backgroundColor: enabled ? "#4CAF50" : "#9E9E9E",
                  },
                  pressed && { opacity: 0.7 },
                ]}
              >
                <Text style={styles.enabledText}>
                  {enabled ? "ON" : "OFF"}
                </Text>
              </View>
            )}
          </Pressable>
        </View>
      </Card>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  container: {
    flexDirection: "row",
    alignItems: "center",
  },
  header: {
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 12,
    width: 70,
    height: 80,
  },
  title: {
    fontSize: 18,
    fontWeight: "600",
    textAlign: "center",
  },
  titleSelected: {
    color: "#1976D2",
  },
  enabledBadge: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
  },
  enabledText: {
    color: "#fff",
    fontSize: 12,
    fontWeight: "600",
  },
});
