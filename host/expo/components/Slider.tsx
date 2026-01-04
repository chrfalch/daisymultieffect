import React, { useCallback } from "react";
import { View, Text, StyleSheet, LayoutChangeEvent } from "react-native";
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  runOnJS,
} from "react-native-reanimated";
import { Gesture, GestureDetector } from "react-native-gesture-handler";

interface SliderProps {
  label: string;
  value: number;
  min?: number;
  max?: number;
  onValueChange?: (value: number) => void;
  onValueChangeEnd?: (value: number) => void;
  updateIntervalMs?: number;
}

export const Slider: React.FC<SliderProps> = ({
  label,
  value,
  min = 0,
  max = 127,
  onValueChange,
  onValueChangeEnd,
  updateIntervalMs = 30,
}) => {
  const width = useSharedValue(0);
  const translateX = useSharedValue(0);
  const startX = useSharedValue(0);
  const currentValue = useSharedValue(value);
  const clamp = (val: number, minVal: number, maxVal: number) => {
    "worklet";
    return Math.min(Math.max(val, minVal), maxVal);
  };

  const clampedValue = (val: number) => clamp(val, min, max);

  const [displayValue, setDisplayValue] = React.useState(() =>
    clampedValue(value)
  );

  const pendingChangeRef = React.useRef<number | null>(null);
  const changeTimerRef = React.useRef<ReturnType<typeof setTimeout> | null>(
    null
  );

  React.useEffect(() => {
    return () => {
      if (changeTimerRef.current) {
        clearTimeout(changeTimerRef.current);
        changeTimerRef.current = null;
      }
    };
  }, []);

  // Update shared value when prop changes
  React.useEffect(() => {
    const next = clampedValue(value);
    currentValue.value = next;
    setDisplayValue(next);
  }, [value, currentValue]);

  const valueToPosition = (val: number) => {
    "worklet";
    if (width.value <= 0 || max === min) return 0;
    return ((val - min) / (max - min)) * width.value;
  };

  const positionToValue = (pos: number) => {
    "worklet";
    if (width.value <= 0 || max === min) return min;
    const normalized = clamp(pos / width.value, 0, 1);
    return Math.round(min + normalized * (max - min));
  };

  const updateValue = useCallback(
    (newValue: number) => {
      onValueChange?.(newValue);
    },
    [onValueChange]
  );

  const updateDisplayValue = useCallback((newValue: number) => {
    setDisplayValue(newValue);
  }, []);

  const scheduleValueChange = useCallback(
    (newValue: number) => {
      pendingChangeRef.current = newValue;
      if (changeTimerRef.current) return;

      changeTimerRef.current = setTimeout(() => {
        changeTimerRef.current = null;
        if (pendingChangeRef.current === null) return;
        updateValue(pendingChangeRef.current);
      }, updateIntervalMs);
    },
    [updateIntervalMs, updateValue]
  );

  const flushValueChange = useCallback(
    (valueToFlush?: number) => {
      if (changeTimerRef.current) {
        clearTimeout(changeTimerRef.current);
        changeTimerRef.current = null;
      }
      const v = valueToFlush ?? pendingChangeRef.current;
      pendingChangeRef.current = null;
      if (typeof v === "number") updateValue(v);
    },
    [updateValue]
  );

  const finishUpdate = useCallback(
    (newValue: number) => {
      onValueChangeEnd?.(newValue);
    },
    [onValueChangeEnd]
  );

  const panGesture = Gesture.Pan()
    .onStart((e) => {
      "worklet";
      startX.value = e.x;
      translateX.value = e.x;
      const newValue = positionToValue(e.x);
      currentValue.value = newValue;
      runOnJS(updateDisplayValue)(newValue);
      runOnJS(flushValueChange)(newValue);
    })
    .onUpdate((e) => {
      "worklet";
      translateX.value = clamp(e.x, 0, width.value);
      const newValue = positionToValue(translateX.value);
      if (newValue !== currentValue.value) {
        currentValue.value = newValue;
        runOnJS(updateDisplayValue)(newValue);
        runOnJS(scheduleValueChange)(newValue);
      }
    })
    .onEnd(() => {
      "worklet";
      runOnJS(flushValueChange)(currentValue.value);
      runOnJS(finishUpdate)(currentValue.value);
    });

  const tapGesture = Gesture.Tap().onEnd((e) => {
    "worklet";
    translateX.value = clamp(e.x, 0, width.value);
    const newValue = positionToValue(translateX.value);
    currentValue.value = newValue;
    runOnJS(updateDisplayValue)(newValue);
    runOnJS(flushValueChange)(newValue);
    runOnJS(finishUpdate)(newValue);
  });

  const gesture = Gesture.Race(panGesture, tapGesture);

  const fillStyle = useAnimatedStyle(() => {
    const fillWidth = valueToPosition(currentValue.value);
    return {
      width: fillWidth,
    };
  });

  const handleLayout = (e: LayoutChangeEvent) => {
    width.value = e.nativeEvent.layout.width;
  };

  return (
    <View style={styles.container}>
      <GestureDetector gesture={gesture}>
        <View style={styles.sliderContainer} onLayout={handleLayout}>
          <Animated.View style={[styles.fill, fillStyle]} />
          <View style={styles.labelContainer}>
            <Text style={styles.label}>{label}</Text>
          </View>
          <View style={styles.valueContainer}>
            <Text style={styles.value}>{displayValue}</Text>
          </View>
        </View>
      </GestureDetector>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    marginVertical: 6,
  },
  sliderContainer: {
    height: 56,
    backgroundColor: "#2a2a3e",
    borderRadius: 12,
    overflow: "hidden",
    flexDirection: "row",
    alignItems: "center",
  },
  fill: {
    position: "absolute",
    left: 0,
    top: 0,
    bottom: 0,
    backgroundColor: "#5b8def",
    borderRadius: 12,
  },
  labelContainer: {
    flex: 1,
    paddingLeft: 16,
    zIndex: 1,
  },
  label: {
    color: "#fff",
    fontSize: 18,
    fontWeight: "500",
  },
  valueContainer: {
    paddingRight: 16,
    zIndex: 1,
  },
  value: {
    color: "#fff",
    fontSize: 18,
    fontWeight: "400",
  },
});

export default Slider;
