import React from "react";
import { View, Text, StyleSheet } from "react-native";
import { Slider } from "./Slider";
import { HStack, VStack } from "./Stack";

interface GainControlProps {
  inputGainDb: number;
  outputGainDb: number;
  onInputGainChange: (gainDb: number) => void;
  onOutputGainChange: (gainDb: number) => void;
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
  onInputGainChange,
  onOutputGainChange,
}) => {
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
      </HStack>
    </VStack>
  );
};

const styles = StyleSheet.create({
  sliderContainer: {
    flex: 1,
  },
});
