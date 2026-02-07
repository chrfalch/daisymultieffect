import React from "react";
import { Text, StyleSheet, View } from "react-native";
import type { EffectParam, EffectSlot } from "../../modules/daisy-multi-fx";
import { Slider } from "../../components/Slider";
import { EnumPicker } from "../../components/EnumPicker";
import { VStack } from "../../components/Stack";

// ParamValueKind enum values (must match firmware base_effect.h)
const ParamValueKind = {
  Number: 0,
  Enum: 1,
  File: 2,
} as const;

export const ParametersPanel: React.FC<{
  slot: EffectSlot;
  getParamName: (typeId: number, paramId: number) => string;
  getParamMeta: (typeId: number, paramId: number) => EffectParam | undefined;
  setSlotParam: (slot: number, paramId: number, value: number) => void;
  effectParams?: EffectParam[];
  outputParams?: { id: number; value: number }[];
}> = ({ slot, getParamName, getParamMeta, setSlotParam, effectParams, outputParams }) => {
  const formatValueFromRange = React.useCallback(
    (
      raw: number,
      meta?: {
        range?: { min: number; max: number; step: number };
        unitPrefix?: string;
        unitSuffix?: string;
      },
    ) => {
      const range = meta?.range;
      if (!range) return "--";

      const v01 = raw / 127;
      const unclamped = range.min + v01 * (range.max - range.min);
      const step = range.step;
      const stepped =
        step > 0
          ? Math.round((unclamped - range.min) / step) * step + range.min
          : unclamped;

      const decimals =
        step > 0 && step < 1
          ? Math.min(6, Math.max(0, Math.ceil(-Math.log10(step))))
          : 0;
      const valueText = stepped.toFixed(decimals);
      const prefix = meta?.unitPrefix ? meta.unitPrefix : "";
      const suffix = meta?.unitSuffix ? ` ${meta.unitSuffix}` : "";
      return `${prefix}${valueText}${suffix}`;
    },
    [],
  );

  // Format a readonly output param value for display
  const formatOutputValue = React.useCallback(
    (value: number, meta: EffectParam) => {
      if (meta.kind === ParamValueKind.Enum && meta.enumOptions) {
        const rounded = Math.round(value);
        const opt = meta.enumOptions.find((o) => o.value === rounded);
        return opt?.name ?? "--";
      }
      if (meta.range) {
        const step = meta.range.step;
        const stepped =
          step > 0 ? Math.round(value / step) * step : value;
        const decimals =
          step > 0 && step < 1
            ? Math.min(6, Math.max(0, Math.ceil(-Math.log10(step))))
            : 0;
        const valueText = stepped.toFixed(decimals);
        const prefix = meta.unitPrefix ? meta.unitPrefix : "";
        const suffix = meta.unitSuffix ? ` ${meta.unitSuffix}` : "";
        return `${prefix}${valueText}${suffix}`;
      }
      return value.toFixed(1);
    },
    [],
  );

  // Collect readonly params from effect metadata
  const readonlyParams = React.useMemo(() => {
    if (!effectParams || !outputParams) return [];
    return effectParams
      .filter((p) => p.isReadonly)
      .map((p) => {
        const outputValue = outputParams.find((op) => op.id === p.id);
        return { meta: p, value: outputValue?.value ?? 0 };
      });
  }, [effectParams, outputParams]);

  const hasWritableParams = Object.keys(slot.params).length > 0;
  const hasReadonlyParams = readonlyParams.length > 0;

  if (!hasWritableParams && !hasReadonlyParams) {
    return <Text style={styles.todoText}>No parameters for this effect.</Text>;
  }

  return (
    <VStack gap={12}>
      {/* Readonly output params (e.g., tuner note/cents) */}
      {readonlyParams.map(({ meta, value }) =>
        meta.kind === ParamValueKind.Number && meta.range ? (
          <Slider
            key={`ro-${meta.id}`}
            label={meta.name}
            description={meta.description}
            value={value}
            min={meta.range.min}
            max={meta.range.max}
            step={meta.range.step}
            disabled
            centerOrigin
            formatValue={(v) => formatOutputValue(v, meta)}
          />
        ) : (
          <View key={`ro-${meta.id}`} style={styles.readonlyParam}>
            <Text style={styles.readonlyLabel}>{meta.name}</Text>
            <Text style={styles.readonlyValue}>
              {formatOutputValue(value, meta)}
            </Text>
          </View>
        ),
      )}

      {/* Writable params */}
      {Object.entries(slot.params).map(([paramId, value]) => {
        const id = Number(paramId);
        const name = getParamName(slot.typeId, id);
        if (!name) return null;

        const meta = getParamMeta(slot.typeId, id);

        // Skip readonly params here (they're rendered above)
        if (meta?.isReadonly) return null;

        // Render enum picker for Enum parameters
        if (meta?.kind === ParamValueKind.Enum && meta.enumOptions) {
          return (
            <EnumPicker
              key={paramId}
              label={name}
              description={meta.description}
              value={value}
              options={meta.enumOptions}
              onValueChange={(newValue) =>
                setSlotParam(slot.slotIndex, id, newValue)
              }
            />
          );
        }

        // Default: render slider for Number parameters
        return (
          <Slider
            key={paramId}
            label={name}
            description={meta?.description}
            value={value}
            min={0}
            max={127}
            formatValue={(raw) => formatValueFromRange(raw, meta)}
            onValueChange={(newValue) =>
              setSlotParam(slot.slotIndex, id, newValue)
            }
          />
        );
      })}
    </VStack>
  );
};

const styles = StyleSheet.create({
  todoText: {
    fontSize: 14,
    color: "#666",
  },
  readonlyParam: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    paddingVertical: 8,
    paddingHorizontal: 4,
  },
  readonlyLabel: {
    fontSize: 14,
    color: "#999",
  },
  readonlyValue: {
    fontSize: 18,
    fontWeight: "600",
    color: "#333",
  },
});
