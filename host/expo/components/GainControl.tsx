import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Slider } from "./Slider";
import { HStack, VStack } from "./Stack";
import { useThemeColors } from "./ThemeProvider";

interface GainControlProps {
  inputGainDb: number;
  outputGainDb: number;
  globalBypass: boolean;
  onInputGainChange: (gainDb: number) => void;
  onOutputGainChange: (gainDb: number) => void;
  onGlobalBypassChange: (bypass: boolean) => void;
}

/**
 * Format gain value with dB suffix and sign
 */
function formatGainDb(value: number): string {
  const sign = value > 0 ? "+" : "";
  return `${sign}${value.toFixed(1)} dB`;
}

export const GainControl: React.FC<GainControlProps> = ({
  inputGainDb,
  outputGainDb,
  globalBypass,
  onInputGainChange,
  onOutputGainChange,
  onGlobalBypassChange,
}) => {
  const colors = useThemeColors();

  return (
    <VStack gap={12}>
      <HStack gap={16} align="stretch">
        <View style={styles.sliderContainer}>
          <Slider
            label="Input Gain"
            description="Boost instrument level signal (0 to +24 dB)"
            value={inputGainDb}
            min={0}
            max={24}
            step={0.5}
            formatValue={formatGainDb}
            onValueChange={onInputGainChange}
          />
        </View>
        <View style={styles.sliderContainer}>
          <Slider
            label="Output Gain"
            description="Adjust final output level (-12 to +12 dB)"
            value={outputGainDb}
            min={-12}
            max={12}
            step={0.5}
            formatValue={formatGainDb}
            onValueChange={onOutputGainChange}
          />
        </View>
        <Pressable
          onPress={() => onGlobalBypassChange(!globalBypass)}
          style={[
            styles.bypassButton,
            {
              backgroundColor: globalBypass
                ? colors.bypassActive
                : colors.bypassInactive,
              borderColor: globalBypass
                ? colors.bypassActive
                : colors.bypassInactive,
            },
          ]}
        >
          <Text
            style={[
              styles.bypassButtonText,
              {
                color: globalBypass
                  ? colors.bypassActiveText
                  : colors.bypassInactiveText,
              },
            ]}
          >
            BYPASS
          </Text>
        </Pressable>
      </HStack>
    </VStack>
  );
};

const styles = StyleSheet.create({
  sliderContainer: {
    flex: 1,
  },
  bypassButton: {
    paddingHorizontal: 16,
    marginVertical: 6,
    borderRadius: 12,
    borderCurve: "continuous",
    borderWidth: 2,
    justifyContent: "center",
    alignItems: "center",
    alignSelf: "stretch",
  },
  bypassButtonText: {
    fontSize: 13,
    fontWeight: "700",
  },
});
