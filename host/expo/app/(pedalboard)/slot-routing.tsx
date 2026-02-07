import React from "react";
import { View, Text, ScrollView } from "react-native";
import { useLocalSearchParams } from "expo-router";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { RoutingPanel } from "../../screens/editor/RoutingPanel";
import { useThemeColors } from "../../components/ThemeProvider";

export default function SlotRoutingSheet() {
  const colors = useThemeColors();
  const { slotIndex: slotIndexStr } = useLocalSearchParams<{
    slotIndex: string;
  }>();
  const slotIndex = Number(slotIndexStr ?? 0);

  const {
    patch,
    getEffectName,
    setSlotRouting,
    setSlotSumToMono,
    setSlotMix,
    setSlotChannelPolicy,
  } = useDaisyMultiFX();

  const slot =
    patch?.slots.find((s) => s.slotIndex === slotIndex) ?? null;

  const effectName = slot ? getEffectName(slot.typeId) : "Routing";

  if (!slot || !patch) {
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
          {effectName} — Routing
        </Text>
        <RoutingPanel
          patch={patch}
          slot={slot}
          getEffectName={getEffectName}
          setSlotRouting={setSlotRouting}
          setSlotSumToMono={setSlotSumToMono}
          setSlotMix={setSlotMix}
          setSlotChannelPolicy={setSlotChannelPolicy}
        />
      </ScrollView>
    </View>
  );
}
