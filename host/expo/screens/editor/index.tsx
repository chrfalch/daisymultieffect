import React from "react";
import { Text, StyleSheet, ScrollView, Pressable } from "react-native";
import { GestureHandlerRootView } from "react-native-gesture-handler";
import Ionicons from "@expo/vector-icons/Ionicons";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { PedalSlot } from "../../components/PedalSlot";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { Button } from "../../components/Button";
import { HStack, VStack, WrapStack } from "../../components/Stack";
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
  const color = selected ? "#fff" : "#1976D2";
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={label}
      style={({ pressed }) => [
        styles.panelTabButton,
        selected && styles.panelTabButtonSelected,
        pressed && styles.panelTabButtonPressed,
      ]}
    >
      <HStack gap={6} align="center">
        <Ionicons name={icon} size={16} color={color} />
        <Text
          style={[
            styles.panelTabButtonText,
            selected && styles.panelTabButtonTextSelected,
          ]}
        >
          {label}
        </Text>
      </HStack>
    </Pressable>
  );
};

export const EditorScreen: React.FC = () => {
  const {
    isConnected,
    connectionStatus,
    patch,
    effectMeta,
    refreshPatch,
    refreshEffectMeta,
    setSlotEnabled,
    setSlotType,
    setSlotParam,
    setSlotRouting,
    setSlotSumToMono,
    setSlotMix,
    setSlotChannelPolicy,
    getEffectName,
    getEffectShortName,
    getParamName,
    getParamMeta,
  } = useDaisyMultiFX();
  const [expandedSlot, setExpandedSlot] = React.useState<number>(0);
  const [panelTab, setPanelTab] = React.useState<PanelTab>("parameters");

  const selectSlot = (slotIndex: number) => {
    setExpandedSlot(slotIndex);
    setPanelTab("parameters");
  };
  const slot =
    patch?.slots.find((s) => s.slotIndex === expandedSlot) ??
    patch?.slots[0] ??
    null;

  const selectedEffectDescription = React.useMemo(() => {
    if (!slot) return undefined;
    return effectMeta.find((e) => e.typeId === slot.typeId)?.description;
  }, [effectMeta, slot?.typeId]);

  return (
    <GestureHandlerRootView style={styles.flex}>
      <ScrollView
        style={styles.container}
        contentContainerStyle={styles.scrollContent}
      >
        <Card>
          <CardTitle>Connection Status</CardTitle>
          <HStack justify="space-between" align="center">
            <VStack>
              <ConnectionStatus
                isConnected={isConnected}
                connectionStatus={connectionStatus}
              />
            </VStack>
            <HStack gap={8}>
              <Button title="Refresh Patch" onPress={refreshPatch} />
              <Button title="Refresh Effects" onPress={refreshEffectMeta} />
            </HStack>
          </HStack>
        </Card>

        {/* Slots */}
        <WrapStack gap={10} justify="center">
          {patch?.slots.map((current, index) => {
            if (index >= patch.numSlots) {
              return null;
            }
            const isSelected = current.slotIndex === expandedSlot;
            return (
              <PedalSlot
                key={current.slotIndex}
                shortName={
                  getEffectShortName(current.typeId) ||
                  `Slot ${current.slotIndex + 1}`
                }
                name={
                  getEffectName(current.typeId) ||
                  `Slot ${current.slotIndex + 1}`
                }
                enabled={current.enabled}
                selected={isSelected}
                onPress={() => selectSlot(current.slotIndex)}
                onToggleEnabled={() =>
                  setSlotEnabled(current.slotIndex, !current.enabled)
                }
              />
            );
          })}
        </WrapStack>

        {/* Empty State */}
        {!patch && (
          <VStack align="center">
            <Text style={styles.emptyStateText}>
              No patch data received yet.
            </Text>
            <Text style={styles.emptyStateHint}>
              Make sure the VST or hardware is running.
            </Text>
          </VStack>
        )}
      </ScrollView>

      {slot && (
        <VStack padding={16}>
          <HStack justify="space-between" align="center">
            <HStack gap={8} align="center" style={styles.headerLeft}>
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
            <Text style={styles.effectDescription} numberOfLines={2}>
              {selectedEffectDescription}
            </Text>
          )}

          {/** Keep the panel height stable by always rendering Parameters (it defines height). */}
          {/** Effect/Routing render as an overlay on top when selected. */}
          <VStack style={[styles.paramsContainer, styles.panelBody]}>
            <VStack
              pointerEvents={panelTab === "parameters" ? "auto" : "none"}
              style={
                panelTab === "parameters" ? undefined : styles.hiddenContent
              }
            >
              <ParametersPanel
                slot={slot}
                getParamName={getParamName}
                getParamMeta={getParamMeta}
                setSlotParam={setSlotParam}
              />
            </VStack>

            {panelTab === "routing" && (
              <RoutingPanel
                patch={patch!}
                slot={slot}
                getEffectShortName={getEffectShortName}
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

const styles = StyleSheet.create({
  flex: {
    flex: 1,
  },
  container: {
    flex: 1,
    backgroundColor: "#f5f5f5",
  },
  scrollContent: {
    padding: 16,
    gap: 16,
  },
  parameterPanelTitle: {
    fontSize: 18,
    fontWeight: "600",
  },
  headerLeft: {
    flex: 1,
    marginRight: 8,
  },
  effectDescription: {
    marginTop: 6,
    fontSize: 13,
    color: "#666",
  },
  paramsContainer: {
    marginTop: 12,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: "#eee",
  },
  panelBody: {
    position: "relative",
  },
  hiddenContent: {
    opacity: 0,
  },
  overlayPanel: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
  },
  panelTabButton: {
    backgroundColor: "#E3F2FD",
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 16,
  },
  panelTabButtonSelected: {
    backgroundColor: "#2196F3",
  },
  panelTabButtonPressed: {
    opacity: 0.7,
  },
  panelTabButtonText: {
    color: "#1976D2",
    fontSize: 13,
  },
  panelTabButtonTextSelected: {
    color: "#fff",
    fontWeight: "600",
  },
  emptyStateText: {
    fontSize: 16,
    color: "#666",
    marginBottom: 8,
  },
  emptyStateHint: {
    fontSize: 14,
    color: "#999",
  },
});
