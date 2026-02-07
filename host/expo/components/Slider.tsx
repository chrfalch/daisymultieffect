import React, { useCallback, useLayoutEffect, useRef } from "react";
import { View, Text, LayoutChangeEvent } from "react-native";
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  runOnJS,
} from "react-native-reanimated";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { useColors } from "../hooks/useColors";

interface SliderProps {
  label: string;
  description?: string;
  value: number;
  min?: number;
  max?: number;
  step?: number;
  formatValue?: (value: number) => string;
  onValueChange?: (value: number) => void;
  onValueChangeEnd?: (value: number) => void;
  updateIntervalMs?: number;
  /** Disable gesture input (read-only display) */
  disabled?: boolean;
  /** Fill from center instead of left edge (useful for bipolar values like -50..+50) */
  centerOrigin?: boolean;
}

export const Slider: React.FC<SliderProps> = ({
  label,
  description,
  value,
  min = 0,
  max = 127,
  step,
  formatValue,
  onValueChange,
  onValueChangeEnd,
  updateIntervalMs = 30,
  disabled = false,
  centerOrigin = false,
}) => {
  const colors = useColors();
  const width = useSharedValue(0);
  const translateX = useSharedValue(0);
  const startX = useSharedValue(0);
  const currentValue = useSharedValue(value);
  const clamp = (val: number, minVal: number, maxVal: number) => {
    "worklet";
    return Math.min(Math.max(val, minVal), maxVal);
  };

  const clampedValue = (val: number) => clamp(val, min, max);

  const snapToStep = (val: number) => {
    "worklet";
    if (!step || step <= 0) return val;
    const snapped = Math.round((val - min) / step) * step + min;
    return clamp(snapped, min, max);
  };

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
    const v = snapToStep(clamp(val, min, max));
    return ((v - min) / (max - min)) * width.value;
  };

  const positionToValue = (pos: number) => {
    "worklet";
    if (width.value <= 0 || max === min) return min;
    const normalized = clamp(pos / width.value, 0, 1);
    const raw = min + normalized * (max - min);
    if (step && step > 0) return snapToStep(raw);
    return Math.round(raw);
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

  const gesture = disabled
    ? Gesture.Tap().enabled(false)
    : Gesture.Race(panGesture, tapGesture);

  const fillStyle = useAnimatedStyle(() => {
    if (centerOrigin) {
      const center = width.value / 2;
      const pos = valueToPosition(currentValue.value);
      const left = Math.min(center, pos);
      const fillWidth = Math.abs(pos - center);
      return { left, width: fillWidth };
    }
    return { width: valueToPosition(currentValue.value) };
  });

  const handleLayout = (e: LayoutChangeEvent) => {
    width.value = e.nativeEvent.layout.width;
  };

  const viewRef = useRef<View>(null);
  useLayoutEffect(() => {
    viewRef.current?.measure((x, y, w, h, pageX, pageY) => {
      width.value = w;
    });
  }, []);

  return (
    <View style={{ marginVertical: 6 }}>
      <GestureDetector gesture={gesture}>
        <View
          style={{
            minHeight: 56,
            paddingVertical: 8,
            backgroundColor: colors.surface,
            borderWidth: 2,
            borderColor: colors.border,
            borderRadius: 12,
            overflow: "hidden",
            flexDirection: "row",
            alignItems: "center",
          }}
          onLayout={handleLayout}
          ref={viewRef}
        >
          <Animated.View
            style={[
              {
                position: "absolute",
                left: 0,
                top: 0,
                bottom: 0,
                backgroundColor: colors.primaryTint,
                borderRadius: 12,
              },
              fillStyle,
            ]}
          />
          <View style={{ flex: 1, paddingLeft: 16, zIndex: 1 }}>
            <Text style={{ color: colors.text, fontSize: 18, fontWeight: "500" }}>
              {label}
            </Text>
            {!!description && (
              <Text
                style={{ marginTop: 2, color: colors.textSecondary, fontSize: 12, fontWeight: "400" }}
                numberOfLines={1}
              >
                {description}
              </Text>
            )}
          </View>
          <View style={{ paddingRight: 16, zIndex: 1 }}>
            <Text style={{ color: colors.text, fontSize: 18, fontWeight: "400" }}>
              {formatValue ? formatValue(displayValue) : String(displayValue)}
            </Text>
          </View>
        </View>
      </GestureDetector>
    </View>
  );
};

export default Slider;
