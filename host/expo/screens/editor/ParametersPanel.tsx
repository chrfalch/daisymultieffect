import React from "react";
import { Text, StyleSheet } from "react-native";
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
}> = ({ slot, getParamName, getParamMeta, setSlotParam }) => {
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

  if (Object.keys(slot.params).length === 0) {
    return <Text style={styles.todoText}>No parameters for this effect.</Text>;
  }

  return (
    <VStack gap={12}>
      {Object.entries(slot.params).map(([paramId, value]) => {
        const id = Number(paramId);
        const name = getParamName(slot.typeId, id);
        if (!name) return null;

        const meta = getParamMeta(slot.typeId, id);

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
});
