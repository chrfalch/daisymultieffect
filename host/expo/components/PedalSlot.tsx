import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Card } from "./Card";

/** Pedal slot dimensions - shared with routing graph layout */
export const PEDAL_SLOT_WIDTH = 90;
export const PEDAL_SLOT_HEIGHT = 120;

interface PedalSlotProps {
  name: string;
  shortName: string;
  enabled: boolean;
  selected?: boolean;
  onPress: () => void;
  onToggleEnabled?: () => void;
  showSwitch?: boolean;
  /** Sum L+R to mono before processing */
  sumToMono?: boolean;
  /** Channel policy: 0=Auto, 1=Mono, 2=Stereo */
  channelPolicy?: number;
}

const ChannelIndicator: React.FC<{
  sumToMono: boolean;
  channelPolicy: number;
  selected: boolean;
  enabled: boolean;
}> = ({ sumToMono, channelPolicy, selected, enabled }) => {
  // Policy: 0=Auto (A), 1=Mono (M), 2=Stereo (S)
  const policyLabel =
    channelPolicy === 1 ? "M" : channelPolicy === 2 ? "S" : "A";
  // Blue when selected (even if disabled), gray when disabled and not selected
  const textColor = selected ? "#1976D2" : !enabled ? "#999" : "#666";
  const bgColor = selected ? "#E3F2FD" : !enabled ? "#F0F0F0" : "#F5F5F5";
  const borderColor = selected ? "#90CAF9" : !enabled ? "#D0D0D0" : "#E0E0E0";

  return (
    <View style={styles.channelIndicator}>
      {sumToMono && (
        <View
          style={[
            styles.indicatorBadge,
            { backgroundColor: bgColor, borderColor },
          ]}
        >
          <Text style={[styles.indicatorText, { color: textColor }]}>Σ</Text>
        </View>
      )}
      <View
        style={[
          styles.indicatorBadge,
          { backgroundColor: bgColor, borderColor },
        ]}
      >
        <Text style={[styles.indicatorText, { color: textColor }]}>
          {policyLabel}
        </Text>
      </View>
    </View>
  );
};

export const PedalSlot: React.FC<PedalSlotProps> = ({
  name,
  shortName,
  enabled,
  selected = false,
  onToggleEnabled,
  showSwitch = true,
  onPress,
  sumToMono = false,
  channelPolicy = 0,
}) => {
  // Lighter gray header when disabled, blue when selected, light blue otherwise
  // Text stays blue when selected even if disabled
  const topStripBg = !enabled ? "#E0E0E0" : selected ? "#2196F3" : "#E3F2FD";
  const topStripText = selected
    ? enabled
      ? "#fff"
      : "#1976D2"
    : !enabled
    ? "#999"
    : "#1976D2";

  // Check if this is an empty slot (no effect assigned)
  const isEmpty = !name || name === "Empty" || shortName === "--";

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
              style={[
                styles.name,
                selected && styles.nameSelected,
                !enabled && !selected && styles.nameDisabled,
              ]}
              numberOfLines={2}
            >
              {name}
            </Text>

            <ChannelIndicator
              sumToMono={sumToMono}
              channelPolicy={channelPolicy}
              selected={selected}
              enabled={enabled}
            />

            {showSwitch && onToggleEnabled ? (
              <Pressable
                onPress={isEmpty ? undefined : onToggleEnabled}
                accessibilityRole="button"
                accessibilityLabel={`Turn ${name} ${enabled ? "off" : "on"}`}
              >
                {({ pressed }) => (
                  <View
                    style={[
                      styles.footswitchOuter,
                      {
                        borderColor: enabled
                          ? "#2196F3"
                          : isEmpty
                          ? "#E0E0E0"
                          : "#D0D0D0",
                      },
                      !isEmpty && pressed && { opacity: 0.7 },
                    ]}
                  >
                    <View
                      style={[
                        styles.footswitchInner,
                        {
                          borderColor: enabled
                            ? "#1976D2"
                            : isEmpty
                            ? "#E0E0E0"
                            : "#9E9E9E",
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
    width: PEDAL_SLOT_WIDTH,
    height: PEDAL_SLOT_HEIGHT,
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
    paddingTop: 8,
    paddingBottom: 10,
    alignItems: "center",
    justifyContent: "space-between",
    gap: 4,
  },
  name: {
    fontSize: 10,
    textAlign: "center",
  },
  nameSelected: {
    color: "#1976D2",
    fontWeight: "600",
  },
  nameDisabled: {
    color: "#999",
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

  channelIndicator: {
    flexDirection: "row",
    gap: 3,
    alignItems: "center",
    justifyContent: "center",
  },
  indicatorBadge: {
    paddingHorizontal: 5,
    paddingVertical: 2,
    borderRadius: 4,
    borderWidth: 1,
  },
  indicatorText: {
    fontSize: 9,
    fontWeight: "600",
  },
});
