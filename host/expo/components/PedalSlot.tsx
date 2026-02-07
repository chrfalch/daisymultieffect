import React from "react";
import { View, Text, Pressable } from "react-native";
import { Card } from "./Card";
import { useColors, type ColorTokens } from "../hooks/useColors";

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
  colors: ColorTokens;
}> = ({ sumToMono, channelPolicy, selected, enabled, colors }) => {
  // Policy: 0=Auto (A), 1=Mono (M), 2=Stereo (S)
  const policyLabel =
    channelPolicy === 1 ? "M" : channelPolicy === 2 ? "S" : "A";
  // Blue when selected (even if disabled), gray when disabled and not selected
  const textColor = selected ? colors.primaryDark : !enabled ? colors.textDisabled : colors.textSecondary;
  const bgColor = selected ? colors.primaryTint : !enabled ? colors.surfaceRecessed : colors.surfaceRecessed;
  const borderColor = selected ? colors.primaryLight : !enabled ? colors.disabledBorder : colors.borderLight;

  return (
    <View style={{ flexDirection: "row", gap: 3, alignItems: "center", justifyContent: "center" }}>
      {sumToMono && (
        <View
          style={{
            paddingHorizontal: 5,
            paddingVertical: 2,
            borderRadius: 4,
            borderWidth: 1,
            backgroundColor: bgColor,
            borderColor,
          }}
        >
          <Text style={{ fontSize: 9, fontWeight: "600", color: textColor }}>Σ</Text>
        </View>
      )}
      <View
        style={{
          paddingHorizontal: 5,
          paddingVertical: 2,
          borderRadius: 4,
          borderWidth: 1,
          backgroundColor: bgColor,
          borderColor,
        }}
      >
        <Text style={{ fontSize: 9, fontWeight: "600", color: textColor }}>
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
  const colors = useColors();

  // Lighter gray header when disabled, blue when selected, light blue otherwise
  // Text stays blue when selected even if disabled
  const topStripBg = !enabled ? colors.disabledBg : selected ? colors.primary : colors.primaryTint;
  const topStripText = selected
    ? enabled
      ? "#fff"
      : colors.primaryDark
    : !enabled
    ? colors.textDisabled
    : colors.primaryDark;

  // Check if this is an empty slot (no effect assigned)
  const isEmpty = !name || name === "Empty" || shortName === "--";

  return (
    <Pressable
      style={{ flexDirection: "row", alignItems: "center" }}
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={`Select ${name} details`}
    >
      <Card selected={selected} style={{ padding: 0, overflow: "hidden" }}>
        <View style={{ width: PEDAL_SLOT_WIDTH, height: PEDAL_SLOT_HEIGHT }}>
          <View style={{ height: 28, justifyContent: "center", alignItems: "center", backgroundColor: topStripBg }}>
            <Text
              style={{
                fontSize: 18,
                fontWeight: "700",
                textAlign: "center",
                letterSpacing: 0.5,
                color: topStripText,
              }}
            >
              {shortName}
            </Text>
          </View>

          <View
            style={{
              flex: 1,
              paddingHorizontal: 10,
              paddingTop: 8,
              paddingBottom: 10,
              alignItems: "center",
              justifyContent: "space-between",
              gap: 4,
            }}
          >
            {!subtitle ? (
              <Text
                style={{
                  fontSize: 10,
                  textAlign: "center",
                  color: selected ? colors.primaryDark : !enabled ? colors.textDisabled : colors.text,
                  fontWeight: selected ? "600" : "400",
                }}
                numberOfLines={2}
              >
                {name}
              </Text>
            ) : (
              <Text
                style={{
                  fontSize: 8,
                  textAlign: "center",
                  color: selected ? colors.primaryDark : !enabled ? colors.textDisabled : colors.textTertiary,
                  fontWeight: selected ? "600" : "400",
                }}
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
              colors={colors}
            />

            {showSwitch && onToggleEnabled ? (
              <Pressable
                onPress={isEmpty ? undefined : onToggleEnabled}
                accessibilityRole="button"
                accessibilityLabel={`Turn ${name} ${enabled ? "off" : "on"}`}
              >
                {({ pressed }) => (
                  <View
                    style={{
                      width: 38,
                      height: 38,
                      borderRadius: 19,
                      borderWidth: 2,
                      backgroundColor: colors.surface,
                      justifyContent: "center",
                      alignItems: "center",
                      borderColor: enabled
                        ? colors.primary
                        : isEmpty
                        ? colors.footswitchBorderEmpty
                        : colors.footswitchBorderDisabled,
                      opacity: !isEmpty && pressed ? 0.7 : 1,
                    }}
                  >
                    <View
                      style={{
                        width: 18,
                        height: 18,
                        borderRadius: 9,
                        borderWidth: 2,
                        borderColor: enabled
                          ? colors.primaryDark
                          : isEmpty
                          ? colors.footswitchBorderEmpty
                          : colors.footswitchInnerDisabled,
                        backgroundColor: enabled ? colors.success : colors.surface,
                      }}
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
