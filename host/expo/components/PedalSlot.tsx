import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Card } from "./Card";

interface PedalSlotProps {
  name: string;
  shortName: string;
  enabled: boolean;
  selected?: boolean;
  onPress: () => void;
  onToggleEnabled?: () => void;
  showSwitch?: boolean;
}

export const PedalSlot: React.FC<PedalSlotProps> = ({
  name,
  shortName,
  enabled,
  selected = false,
  onToggleEnabled,
  showSwitch = true,
  onPress,
}) => {
  const topStripBg = selected ? "#2196F3" : "#E3F2FD";
  const topStripText = selected ? "#fff" : "#1976D2";

  return (
    <Pressable
      style={styles.container}
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={`Select ${name} details`}
    >
      <Card selected={selected} style={styles.pedalCard}>
        <View style={styles.pedal}>
          <View style={[styles.topStrip, { backgroundColor: topStripBg }]}>
            <Text style={[styles.shortName, { color: topStripText }]}>
              {shortName}
            </Text>
          </View>

          <View style={styles.face}>
            <Text
              style={[styles.name, selected && styles.nameSelected]}
              numberOfLines={2}
            >
              {name}
            </Text>

            {showSwitch && onToggleEnabled ? (
              <Pressable
                onPress={onToggleEnabled}
                accessibilityRole="button"
                accessibilityLabel={`Turn ${name} ${enabled ? "off" : "on"}`}
              >
                {({ pressed }) => (
                  <View
                    style={[
                      styles.footswitchOuter,
                      {
                        borderColor: enabled ? "#2196F3" : "#9E9E9E",
                      },
                      pressed && { opacity: 0.7 },
                    ]}
                  >
                    <View
                      style={[
                        styles.footswitchInner,
                        {
                          borderColor: enabled ? "#1976D2" : "#9E9E9E",
                          backgroundColor: enabled ? "#4CAF50" : "#fff",
                        },
                      ]}
                    />
                  </View>
                )}
              </Pressable>
            ) : null}
          </View>
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

  pedalCard: {
    padding: 0,
    overflow: "hidden",
  },
  pedal: {
    width: 90,
    height: 120,
  },

  topStrip: {
    height: 28,
    justifyContent: "center",
    alignItems: "center",
  },
  shortName: {
    fontSize: 18,
    fontWeight: "700",
    textAlign: "center",
    letterSpacing: 0.5,
  },

  face: {
    flex: 1,
    paddingHorizontal: 10,
    paddingTop: 10,
    paddingBottom: 12,
    alignItems: "center",
    justifyContent: "space-between",
  },
  name: {
    fontSize: 10,
    textAlign: "center",
  },
  nameSelected: {
    color: "#1976D2",
    fontWeight: "600",
  },

  footswitchOuter: {
    width: 38,
    height: 38,
    borderRadius: 19,
    borderWidth: 2,
    backgroundColor: "#fff",
    justifyContent: "center",
    alignItems: "center",
  },
  footswitchInner: {
    width: 18,
    height: 18,
    borderRadius: 9,
    borderWidth: 2,
    backgroundColor: "#fff",
  },
});
