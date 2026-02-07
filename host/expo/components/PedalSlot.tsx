import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Card } from "./Card";
import { useThemeColors } from "./ThemeProvider";

/** Pedal slot dimensions - shared with routing graph layout */
export const PEDAL_SLOT_WIDTH = 90;
export const PEDAL_SLOT_HEIGHT = 120;

interface PedalSlotProps {
  name: string;
  shortName: string;
  subtitle?: string;
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
  const colors = useThemeColors();
  // Policy: 0=Auto (A), 1=Mono (M), 2=Stereo (S)
  const policyLabel =
    channelPolicy === 1 ? "M" : channelPolicy === 2 ? "S" : "A";
  const textColor = selected ? colors.accentDark : !enabled ? colors.textTertiary : colors.textSecondary;
  const bgColor = selected ? colors.accentLight : !enabled ? colors.surfaceSecondary : colors.surfaceSecondary;
  const borderColor = selected ? colors.accent : !enabled ? colors.border : colors.border;

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
  subtitle,
  enabled,
  selected = false,
  onToggleEnabled,
  showSwitch = true,
  onPress,
  sumToMono = false,
  channelPolicy = 0,
}) => {
  const colors = useThemeColors();
  // Lighter gray header when disabled, blue when selected, light blue otherwise
  // Text stays blue when selected even if disabled
  const topStripBg = !enabled ? colors.bypassInactive : selected ? colors.accent : colors.accentLight;
  const topStripText = selected
    ? enabled
      ? colors.textInverse
      : colors.accentDark
    : !enabled
    ? colors.textTertiary
    : colors.accentDark;

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
            {!subtitle ? (
              <Text
                style={[
                  styles.name,
                  { color: colors.text },
                  selected && { color: colors.accentDark, fontWeight: "600" },
                  !enabled && !selected && { color: colors.textTertiary },
                ]}
                numberOfLines={2}
              >
                {name}
              </Text>
            ) : (
              <Text
                style={[
                  styles.subtitle,
                  { color: colors.textSecondary },
                  selected && { color: colors.accentDark, fontWeight: "600" },
                  !enabled && !selected && { color: colors.textTertiary },
                ]}
                numberOfLines={2}
              >
                {subtitle}
              </Text>
            )}

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
                          ? colors.accent
                          : isEmpty
                          ? colors.border
                          : colors.border,
                      },
                      !isEmpty && pressed && { opacity: 0.7 },
                    ]}
                  >
                    <View
                      style={[
                        styles.footswitchInner,
                        {
                          borderColor: enabled
                            ? colors.accentDark
                            : isEmpty
                            ? colors.border
                            : colors.textTertiary,
                          backgroundColor: enabled ? colors.success : colors.surfacePrimary,
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
  subtitle: {
    fontSize: 8,
    textAlign: "center",
  },

  footswitchOuter: {
    width: 38,
    height: 38,
    borderRadius: 19,
    borderWidth: 2,
    justifyContent: "center",
    alignItems: "center",
  },
  footswitchInner: {
    width: 18,
    height: 18,
    borderRadius: 9,
    borderWidth: 2,
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
