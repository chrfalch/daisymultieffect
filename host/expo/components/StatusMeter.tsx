import React, { useEffect } from "react";
import { View, Text } from "react-native";
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  withTiming,
  Easing,
} from "react-native-reanimated";
import type { DeviceStatus } from "../modules/daisy-multi-fx";
import { useColors } from "../hooks/useColors";

interface StatusMeterProps {
  deviceStatus: DeviceStatus | null;
}

// Timing config for smooth meter movement
const timingConfig = {
  duration: 100,
  easing: Easing.out(Easing.quad),
};

/**
 * Convert linear level (0.0-1.0+) to dB
 */
function linearToDb(level: number): number {
  if (level < 0.0001) return -60; // Floor at -60dB
  return 20 * Math.log10(level);
}

/**
 * Format dB value for display
 */
function formatDb(db: number): string {
  if (db <= -60) return "---";
  return `${db.toFixed(1)} dB`;
}

/**
 * Format CPU percentage for display
 */
function formatCpu(load: number): string {
  return `${(load * 100).toFixed(1)}%`;
}

/**
 * Animated horizontal level meter bar using Reanimated
 */
const LevelBar: React.FC<{ level: number; color: string; trackColor: string }> = ({
  level,
  color,
  trackColor,
}) => {
  // Map dB to 0-1 range: -60dB = 0, 0dB = 1
  const db = linearToDb(level);
  const normalized = Math.max(0, Math.min(1, (db + 60) / 60));

  const widthPercent = useSharedValue(0);

  useEffect(() => {
    widthPercent.value = withTiming(normalized * 100, timingConfig);
  }, [normalized, widthPercent]);

  const animatedStyle = useAnimatedStyle(() => ({
    width: `${widthPercent.value}%`,
  }));

  return (
    <View
      style={{
        flex: 1,
        height: 8,
        backgroundColor: trackColor,
        borderRadius: 4,
        overflow: "hidden",
        position: "relative",
      }}
    >
      <Animated.View
        style={[{ height: "100%", borderRadius: 4, backgroundColor: color }, animatedStyle]}
      />
    </View>
  );
};

/**
 * Animated CPU load bar using Reanimated
 */
const CpuBar: React.FC<{ load: number; maxLoad: number; trackColor: string }> = ({
  load,
  maxLoad,
  trackColor,
}) => {
  const avgPercent = Math.max(0, Math.min(100, load * 100));
  const maxPercent = Math.max(0, Math.min(100, maxLoad * 100));

  // Color based on load: green < 50%, yellow < 80%, red >= 80%
  const color =
    maxLoad >= 0.8 ? "#F44336" : maxLoad >= 0.5 ? "#FF9800" : "#4CAF50";

  const avgWidth = useSharedValue(0);
  const maxLeft = useSharedValue(0);

  useEffect(() => {
    avgWidth.value = withTiming(avgPercent, timingConfig);
  }, [avgPercent, avgWidth]);

  useEffect(() => {
    maxLeft.value = withTiming(maxPercent, timingConfig);
  }, [maxPercent, maxLeft]);

  const avgAnimatedStyle = useAnimatedStyle(() => ({
    width: `${avgWidth.value}%`,
  }));

  const maxAnimatedStyle = useAnimatedStyle(() => ({
    left: `${maxLeft.value}%`,
  }));

  return (
    <View
      style={{
        flex: 1,
        height: 8,
        backgroundColor: trackColor,
        borderRadius: 4,
        overflow: "hidden",
        position: "relative",
      }}
    >
      {/* Max load marker */}
      <Animated.View
        style={[
          {
            position: "absolute",
            top: 0,
            width: 2,
            height: "100%",
            marginLeft: -1,
            backgroundColor: color,
          },
          maxAnimatedStyle,
        ]}
      />
      {/* Average load fill */}
      <Animated.View
        style={[
          { height: "100%", borderRadius: 4, backgroundColor: color, opacity: 0.7 },
          avgAnimatedStyle,
        ]}
      />
    </View>
  );
};

export const StatusMeter: React.FC<StatusMeterProps> = ({ deviceStatus }) => {
  const colors = useColors();

  if (!deviceStatus) {
    return (
      <View style={{ gap: 8 }}>
        <Text style={{ color: colors.textSecondary, fontSize: 12, fontStyle: "italic" }}>
          No device status
        </Text>
      </View>
    );
  }

  const inputDb = linearToDb(deviceStatus.inputLevel);
  const outputDb = linearToDb(deviceStatus.outputLevel);

  return (
    <View style={{ gap: 8 }}>
      {/* Input Level */}
      <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
        <Text style={{ width: 32, fontSize: 11, fontWeight: "600", color: colors.textSecondary }}>
          IN
        </Text>
        <LevelBar level={deviceStatus.inputLevel} color={colors.primary} trackColor={colors.disabledBg} />
        <Text
          style={{
            width: 90,
            fontSize: 11,
            color: colors.text,
            textAlign: "right",
            fontVariant: ["tabular-nums"],
          }}
        >
          {formatDb(inputDb)}
        </Text>
      </View>

      {/* Output Level */}
      <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
        <Text style={{ width: 32, fontSize: 11, fontWeight: "600", color: colors.textSecondary }}>
          OUT
        </Text>
        <LevelBar level={deviceStatus.outputLevel} color={colors.success} trackColor={colors.disabledBg} />
        <Text
          style={{
            width: 90,
            fontSize: 11,
            color: colors.text,
            textAlign: "right",
            fontVariant: ["tabular-nums"],
          }}
        >
          {formatDb(outputDb)}
        </Text>
      </View>

      {/* CPU Load */}
      <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
        <Text style={{ width: 32, fontSize: 11, fontWeight: "600", color: colors.textSecondary }}>
          CPU
        </Text>
        <CpuBar load={deviceStatus.cpuAvg} maxLoad={deviceStatus.cpuMax} trackColor={colors.disabledBg} />
        <Text
          style={{
            width: 90,
            fontSize: 11,
            color: colors.text,
            textAlign: "right",
            fontVariant: ["tabular-nums"],
          }}
        >
          {formatCpu(deviceStatus.cpuAvg)} / {formatCpu(deviceStatus.cpuMax)}
        </Text>
      </View>
    </View>
  );
};
