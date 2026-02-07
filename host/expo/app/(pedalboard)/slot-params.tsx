import React from "react";
import { View, Text, ScrollView } from "react-native";
import { useLocalSearchParams } from "expo-router";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { useDeviceStatus } from "../../hooks/useDeviceStatus";
import { ParametersPanel } from "../../screens/editor/ParametersPanel";
import { useThemeColors } from "../../components/ThemeProvider";

export default function SlotParamsSheet() {
  const colors = useThemeColors();
  const { slotIndex: slotIndexStr } = useLocalSearchParams<{
    slotIndex: string;
  }>();
  const slotIndex = Number(slotIndexStr ?? 0);

  const { patch, effectMeta, getParamName, getParamMeta, setSlotParam, getEffectName } =
    useDaisyMultiFX();
  const deviceStatus = useDeviceStatus();

  const slot =
    patch?.slots.find((s) => s.slotIndex === slotIndex) ?? null;

  const selectedEffectParams = React.useMemo(() => {
    if (!slot) return undefined;
    return effectMeta.find((e) => e.typeId === slot.typeId)?.params;
  }, [effectMeta, slot?.typeId]);

  const selectedSlotOutputParams = React.useMemo(() => {
    if (!slot || !deviceStatus?.outputParams) return undefined;
    const slotOutput = deviceStatus.outputParams.find(
      (s) => s.slotIndex === slot.slotIndex,
    );
    return slotOutput?.params;
  }, [deviceStatus?.outputParams, slot?.slotIndex]);

  const effectName = slot ? getEffectName(slot.typeId) : "Parameters";
  const effectDescription = React.useMemo(() => {
    if (!slot) return undefined;
    return effectMeta.find((e) => e.typeId === slot.typeId)?.description;
  }, [effectMeta, slot?.typeId]);

  if (!slot) {
    return (
      <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
        <Text style={{ color: colors.textSecondary }}>No slot selected</Text>
      </View>
    );
  }

  return (
    <View style={{ flex: 1 }}>
      <ScrollView
        style={{ flex: 1 }}
        contentContainerStyle={{ padding: 16, gap: 8 }}
      >
        <Text
          style={{
            fontSize: 20,
            fontWeight: "700",
            color: colors.text,
          }}
        >
          {effectName}
        </Text>
        {!!effectDescription && (
          <Text
            style={{
              fontSize: 13,
              color: colors.textSecondary,
              marginBottom: 8,
            }}
            numberOfLines={2}
          >
            {effectDescription}
          </Text>
        )}
        <ParametersPanel
          slot={slot}
          getParamName={getParamName}
          getParamMeta={getParamMeta}
          setSlotParam={setSlotParam}
          effectParams={selectedEffectParams}
          outputParams={selectedSlotOutputParams}
        />
      </ScrollView>
    </View>
  );
}
