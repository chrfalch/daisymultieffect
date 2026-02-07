import React from "react";
import { Text, ScrollView, Pressable, View } from "react-native";
import { router } from "expo-router";
import Ionicons from "@expo/vector-icons/Ionicons";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { GraphView } from "../../components/GraphView";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { DeviceStatusMeter } from "../../components/DeviceStatusMeter";
import { GainControl } from "../../components/GainControl";
import { Button } from "../../components/Button";
import { HStack, VStack } from "../../components/Stack";
import { EffectContextMenuButton } from "../../screens/editor/EffectContextMenuButton";
import { useThemeColors } from "../../components/ThemeProvider";

export default function PedalboardScreen() {
  const colors = useThemeColors();
  const {
    isConnected,
    connectionStatus,
    patch,
    effectMeta,
    refreshPatch,
    setSlotEnabled,
    setSlotType,
    setGlobalBypass,
    setInputGain,
    setOutputGain,
    getEffectName,
    getEffectShortName,
    pushPatchToVst,
    getDisplayLabel,
  } = useDaisyMultiFX();

  const [expandedSlot, setExpandedSlot] = React.useState<number>(0);
  const [globalBypass, setGlobalBypassState] = React.useState(false);

  const handleGlobalBypassChange = React.useCallback(
    (bypass: boolean) => {
      setGlobalBypassState(bypass);
      setGlobalBypass(bypass);
    },
    [setGlobalBypass],
  );

  const selectSlot = (slotIndex: number) => {
    setExpandedSlot(slotIndex);
  };

  const slot =
    patch?.slots.find((s) => s.slotIndex === expandedSlot) ??
    patch?.slots[0] ??
    null;

  const openParams = () => {
    if (!slot) return;
    router.push({
      pathname: "/(pedalboard)/slot-params",
      params: { slotIndex: expandedSlot },
    });
  };

  const openRouting = () => {
    if (!slot) return;
    router.push({
      pathname: "/(pedalboard)/slot-routing",
      params: { slotIndex: expandedSlot },
    });
  };

  return (
    <View style={{ flex: 1, backgroundColor: colors.background }}>
      <ScrollView
        style={{ flex: 1 }}
        contentContainerStyle={{ padding: 16, gap: 16 }}
        contentInsetAdjustmentBehavior="automatic"
      >
        <HStack justify="space-between" align="stretch" gap={16}>
          <Card flex={1}>
            <VStack>
              <CardTitle>Connection</CardTitle>
              <HStack justify="space-between" align="center">
                <VStack>
                  <ConnectionStatus
                    isConnected={isConnected}
                    connectionStatus={connectionStatus}
                  />
                </VStack>
                <VStack gap={8}>
                  <Button title="Load Patch" onPress={refreshPatch} />
                  <Button title="Push Patch" onPress={pushPatchToVst} />
                </VStack>
              </HStack>
            </VStack>
          </Card>
          <Card flex={1}>
            <VStack>
              <CardTitle>Device Status</CardTitle>
              <DeviceStatusMeter />
            </VStack>
          </Card>
        </HStack>

        {patch && (
          <Card>
            <VStack>
              <GainControl
                inputGainDb={patch.inputGainDb}
                outputGainDb={patch.outputGainDb}
                globalBypass={globalBypass}
                onInputGainChange={setInputGain}
                onOutputGainChange={setOutputGain}
                onGlobalBypassChange={handleGlobalBypassChange}
              />
            </VStack>
          </Card>
        )}

        {patch && (
          <Card style={{ paddingVertical: 8 }}>
            <GraphView
              slots={patch.slots}
              numSlots={patch.numSlots}
              selectedSlotIndex={expandedSlot}
              getShortName={getEffectShortName}
              getName={getEffectName}
              onToggleSlotEnabled={setSlotEnabled}
              onSelectSlot={selectSlot}
              getDisplayLabel={getDisplayLabel}
            />
          </Card>
        )}

        {!patch && (
          <VStack align="center">
            <Text style={{ fontSize: 16, color: colors.textSecondary, marginBottom: 8 }}>
              No patch data received yet.
            </Text>
            <Text style={{ fontSize: 14, color: colors.textTertiary }}>
              Make sure the VST or hardware is running.
            </Text>
          </VStack>
        )}

        {/* Spacer for bottom bar */}
        {slot && <View style={{ height: 60 }} />}
      </ScrollView>

      {/* Bottom bar for selected slot actions */}
      {slot && (
        <View
          style={{
            flexDirection: "row",
            alignItems: "center",
            paddingHorizontal: 16,
            paddingVertical: 10,
            gap: 12,
            backgroundColor: colors.surfacePrimary,
            borderTopWidth: 1,
            borderTopColor: colors.border,
          }}
        >
          <View style={{ flex: 1 }}>
            <EffectContextMenuButton
              slot={slot}
              effectMeta={effectMeta}
              setSlotType={setSlotType}
            />
          </View>
          <Pressable
            onPress={openParams}
            style={({ pressed }) => ({
              flexDirection: "row",
              alignItems: "center",
              gap: 6,
              backgroundColor: colors.accentLight,
              paddingHorizontal: 12,
              paddingVertical: 8,
              borderRadius: 16,
              opacity: pressed ? 0.7 : 1,
            })}
          >
            <Ionicons name="options-outline" size={16} color={colors.accentDark} />
            <Text style={{ color: colors.accentDark, fontSize: 13, fontWeight: "500" }}>
              Parameters
            </Text>
          </Pressable>
          <Pressable
            onPress={openRouting}
            style={({ pressed }) => ({
              flexDirection: "row",
              alignItems: "center",
              gap: 6,
              backgroundColor: colors.accentLight,
              paddingHorizontal: 12,
              paddingVertical: 8,
              borderRadius: 16,
              opacity: pressed ? 0.7 : 1,
            })}
          >
            <Ionicons name="git-network-outline" size={16} color={colors.accentDark} />
            <Text style={{ color: colors.accentDark, fontSize: 13, fontWeight: "500" }}>
              Routing
            </Text>
          </Pressable>
        </View>
      )}
    </View>
  );
}
