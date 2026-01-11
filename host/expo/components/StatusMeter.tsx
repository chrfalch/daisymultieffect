import React, { useEffect } from "react";
import { View, Text, StyleSheet } from "react-native";
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  withTiming,
  Easing,
} from "react-native-reanimated";
import type { DeviceStatus } from "../modules/daisy-multi-fx";

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
const LevelBar: React.FC<{ level: number; color: string }> = ({
  level,
  color,
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
    <View style={styles.meterBarContainer}>
      <Animated.View
        style={[styles.meterBarFill, { backgroundColor: color }, animatedStyle]}
      />
    </View>
  );
};

/**
 * Animated CPU load bar using Reanimated
 */
const CpuBar: React.FC<{ load: number; maxLoad: number }> = ({
  load,
  maxLoad,
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
    <View style={styles.meterBarContainer}>
      {/* Max load marker */}
      <Animated.View
        style={[
          styles.cpuMaxMarker,
          { backgroundColor: color },
          maxAnimatedStyle,
        ]}
      />
      {/* Average load fill */}
      <Animated.View
        style={[
          styles.meterBarFill,
          { backgroundColor: color, opacity: 0.7 },
          avgAnimatedStyle,
        ]}
      />
    </View>
  );
};

export const StatusMeter: React.FC<StatusMeterProps> = ({ deviceStatus }) => {
  if (!deviceStatus) {
    return (
      <View style={styles.container}>
        <Text style={styles.noDataText}>No device status</Text>
      </View>
    );
  }

  const inputDb = linearToDb(deviceStatus.inputLevel);
  const outputDb = linearToDb(deviceStatus.outputLevel);

  return (
    <View style={styles.container}>
      {/* Input Level */}
      <View style={styles.meterRow}>
        <Text style={styles.label}>IN</Text>
        <LevelBar level={deviceStatus.inputLevel} color="#2196F3" />
        <Text style={styles.value}>{formatDb(inputDb)}</Text>
      </View>

      {/* Output Level */}
      <View style={styles.meterRow}>
        <Text style={styles.label}>OUT</Text>
        <LevelBar level={deviceStatus.outputLevel} color="#4CAF50" />
        <Text style={styles.value}>{formatDb(outputDb)}</Text>
      </View>

      {/* CPU Load */}
      <View style={styles.meterRow}>
        <Text style={styles.label}>CPU</Text>
        <CpuBar load={deviceStatus.cpuAvg} maxLoad={deviceStatus.cpuMax} />
        <Text style={styles.value}>
          {formatCpu(deviceStatus.cpuAvg)} / {formatCpu(deviceStatus.cpuMax)}
        </Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    gap: 8,
  },
  noDataText: {
    color: "#666",
    fontSize: 12,
    fontStyle: "italic",
  },
  meterRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  label: {
    width: 32,
    fontSize: 11,
    fontWeight: "600",
    color: "#666",
  },
  meterBarContainer: {
    flex: 1,
    height: 8,
    backgroundColor: "#E0E0E0",
    borderRadius: 4,
    overflow: "hidden",
    position: "relative",
  },
  meterBarFill: {
    height: "100%",
    borderRadius: 4,
  },
  cpuMaxMarker: {
    position: "absolute",
    top: 0,
    width: 2,
    height: "100%",
    marginLeft: -1,
  },
  value: {
    width: 90,
    fontSize: 11,
    color: "#333",
    textAlign: "right",
    fontVariant: ["tabular-nums"],
  },
});
