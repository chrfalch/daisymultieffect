import React from "react";
import { Text, ScrollView, Pressable } from "react-native";
import { GestureHandlerRootView } from "react-native-gesture-handler";
import Ionicons from "@expo/vector-icons/Ionicons";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { useDeviceStatus } from "../../hooks/useDeviceStatus";
import { useColors } from "../../hooks/useColors";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { GraphView } from "../../components/GraphView";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { DeviceStatusMeter } from "../../components/DeviceStatusMeter";
import { GainControl } from "../../components/GainControl";
import { Button } from "../../components/Button";
import { HStack, VStack } from "../../components/Stack";
import { ParametersPanel } from "./ParametersPanel";
import { RoutingPanel } from "./RoutingPanel";
import { EffectContextMenuButton } from "./EffectContextMenuButton";

type PanelTab = "parameters" | "routing";

const PanelTabButton: React.FC<{
  label: string;
  icon: React.ComponentProps<typeof Ionicons>["name"];
  selected: boolean;
  onPress: () => void;
}> = ({ label, icon, selected, onPress }) => {
  const colors = useColors();
  const color = selected ? "#fff" : colors.primaryOnTint;
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={label}
      style={({ pressed }) => ({
        backgroundColor: selected ? colors.primary : colors.primaryTint,
        paddingHorizontal: 10,
        paddingVertical: 6,
        borderRadius: 16,
        opacity: pressed ? 0.7 : 1,
      })}
    >
      <HStack gap={6} align="center">
        <Ionicons name={icon} size={16} color={color} />
        <Text
          style={{
            color,
            fontSize: 13,
            fontWeight: selected ? "600" : "400",
          }}
        >
          {label}
        </Text>
      </HStack>
    </Pressable>
  );
};

export const EditorScreen: React.FC = () => {
  const colors = useColors();
  const {
    isConnected,
    connectionStatus,
    patch,
    effectMeta,
    refreshPatch,
    setSlotEnabled,
    setSlotType,
    setSlotParam,
    setSlotRouting,
    setSlotSumToMono,
    setSlotMix,
    setSlotChannelPolicy,
    setGlobalBypass,
    setInputGain,
    setOutputGain,
    getEffectName,
    getEffectShortName,
    getParamName,
    pushPatchToVst,
    getParamMeta,
    getDisplayLabel,
  } = useDaisyMultiFX();
  const deviceStatus = useDeviceStatus();
  const [expandedSlot, setExpandedSlot] = React.useState<number>(0);
  const [panelTab, setPanelTab] = React.useState<PanelTab>("parameters");
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

  const selectedEffectDescription = React.useMemo(() => {
    if (!slot) return undefined;
    return effectMeta.find((e) => e.typeId === slot.typeId)?.description;
  }, [effectMeta, slot?.typeId]);

  // Get all params metadata for the selected effect (needed for readonly params)
  const selectedEffectParams = React.useMemo(() => {
    if (!slot) return undefined;
    return effectMeta.find((e) => e.typeId === slot.typeId)?.params;
  }, [effectMeta, slot?.typeId]);

  // Extract output params for the selected slot from device status
  const selectedSlotOutputParams = React.useMemo(() => {
    if (!slot || !deviceStatus?.outputParams) return undefined;
    const slotOutput = deviceStatus.outputParams.find(
      (s) => s.slotIndex === slot.slotIndex,
    );
    return slotOutput?.params;
  }, [deviceStatus?.outputParams, slot?.slotIndex]);

  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <ScrollView
        style={{ flex: 1, backgroundColor: colors.background }}
        contentContainerStyle={{ padding: 16, gap: 16 }}
      >
        <HStack justify="space-between" align="stretch" gap={16}>
          <Card flex={1}>
            <VStack>
              <CardTitle>Connection Status</CardTitle>
              <HStack justify="space-between" align="center">
                <VStack>
                  <ConnectionStatus
                    isConnected={isConnected}
                    connectionStatus={connectionStatus}
                  />
                </VStack>
                <VStack gap={8}>
                  <Button title="Load Patch from VST" onPress={refreshPatch} />
                  <Button title="Push Patch to VST" onPress={pushPatchToVst} />
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

        {/* Empty State */}
        {!patch && (
          <VStack align="center">
            <Text style={{ fontSize: 16, color: colors.textSecondary, marginBottom: 8 }}>
              No patch data received yet.
            </Text>
            <Text style={{ fontSize: 14, color: colors.textDisabled }}>
              Make sure the VST or hardware is running.
            </Text>
          </VStack>
        )}
      </ScrollView>

      {slot && (
        <VStack padding={16}>
          <HStack justify="space-between" align="center">
            <HStack gap={8} align="center" style={{ flex: 1, marginRight: 8 }}>
              <EffectContextMenuButton
                slot={slot}
                effectMeta={effectMeta}
                setSlotType={setSlotType}
              />
            </HStack>
            <HStack gap={8} align="center">
              <PanelTabButton
                label="Parameters"
                icon="options-outline"
                selected={panelTab === "parameters"}
                onPress={() => setPanelTab("parameters")}
              />
              <PanelTabButton
                label="Routing"
                icon="git-network-outline"
                selected={panelTab === "routing"}
                onPress={() => setPanelTab("routing")}
              />
            </HStack>
          </HStack>
          {!!selectedEffectDescription && (
            <Text
              style={{ marginTop: 6, fontSize: 13, color: colors.textSecondary }}
              numberOfLines={2}
            >
              {selectedEffectDescription}
            </Text>
          )}

          {/** Keep the panel height stable by always rendering Parameters (it defines height). */}
          {/** Effect/Routing render as an overlay on top when selected. */}
          <VStack
            style={{
              marginTop: 12,
              paddingTop: 12,
              borderTopWidth: 1,
              borderTopColor: colors.separator,
              minHeight: 300,
              position: "relative",
            }}
          >
            <VStack
              pointerEvents={panelTab === "parameters" ? "auto" : "none"}
              style={panelTab === "parameters" ? undefined : { opacity: 0 }}
            >
              <ParametersPanel
                slot={slot}
                getParamName={getParamName}
                getParamMeta={getParamMeta}
                setSlotParam={setSlotParam}
                effectParams={selectedEffectParams}
                outputParams={selectedSlotOutputParams}
              />
            </VStack>

            {panelTab === "routing" && (
              <RoutingPanel
                patch={patch!}
                slot={slot}
                getEffectName={getEffectName}
                setSlotRouting={setSlotRouting}
                setSlotSumToMono={setSlotSumToMono}
                setSlotMix={setSlotMix}
                setSlotChannelPolicy={setSlotChannelPolicy}
              />
            )}
          </VStack>
        </VStack>
      )}
    </GestureHandlerRootView>
  );
};
