import React from "react";
import { Text, StyleSheet, ScrollView, Pressable } from "react-native";
import { GestureHandlerRootView } from "react-native-gesture-handler";
import Ionicons from "@expo/vector-icons/Ionicons";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Slider } from "../../components/Slider";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { PedalSlot } from "../../components/PedalSlot";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { Button } from "../../components/Button";
import { Badge } from "../../components/Badge";
import { HStack, VStack, WrapStack } from "../../components/Stack";

type PanelTab = "parameters" | "effect" | "routing";

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
    getEffectName,
    getEffectShortName,
    getParamName,
    getParamMeta,
  } = useDaisyMultiFX();

  const formatValueFromRange = (
    raw: number,
    meta?: {
      range?: { min: number; max: number; step: number };
      unitPrefix?: string;
      unitSuffix?: string;
    }
  ) => {
    const range = meta?.range;
    if (!range) return String(raw);
    const v01 = raw / 127;
    const unclamped = range.min + v01 * (range.max - range.min);
    const step = range.step;
    const stepped =
      step > 0
        ? Math.round((unclamped - range.min) / step) * step + range.min
        : unclamped;

    // Derive a reasonable number of decimals from step
    const decimals =
      step > 0 && step < 1
        ? Math.min(6, Math.max(0, Math.ceil(-Math.log10(step))))
        : 0;
    const valueText = stepped.toFixed(decimals);
    const prefix = meta?.unitPrefix ? meta.unitPrefix : "";
    const suffix = meta?.unitSuffix ? ` ${meta.unitSuffix}` : "";
    return `${prefix}${valueText}${suffix}`;
  };
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
        <WrapStack gap={10}>
          {patch?.slots.map((current, index) => {
            if (index >= patch.numSlots) {
              return null;
            }
            const isSelected = current.slotIndex === expandedSlot;
            return (
              <PedalSlot
                key={current.slotIndex}
                name={
                  getEffectShortName(current.typeId) ||
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
            <Text style={styles.parameterPanelTitle}>
              {getEffectName(slot.typeId) || `Slot ${slot.slotIndex + 1}`}
            </Text>
            <HStack gap={8} align="center">
              <PanelTabButton
                label="Parameters"
                icon="options-outline"
                selected={panelTab === "parameters"}
                onPress={() => setPanelTab("parameters")}
              />
              <PanelTabButton
                label="Effect"
                icon="color-wand-outline"
                selected={panelTab === "effect"}
                onPress={() => setPanelTab("effect")}
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
              {Object.keys(slot.params).length === 0 && (
                <Text style={styles.todoText}>
                  No parameters for this effect.
                </Text>
              )}
              {Object.entries(slot.params).map(([paramId, value]) => {
                const id = Number(paramId);
                const name = getParamName(slot.typeId, id);
                if (!name) return null;

                const meta = getParamMeta(slot.typeId, id);
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

            {panelTab === "effect" && (
              <VStack style={styles.overlayPanel} paddingVertical={16}>
                {effectMeta.length === 0 ? (
                  <Text style={styles.todoText}>
                    No effects metadata loaded yet. Tap “Refresh Effects”.
                  </Text>
                ) : (
                  <WrapStack gap={8}>
                    {effectMeta.map((effect) => {
                      const isSelected = slot?.typeId === effect.typeId;
                      return (
                        <Badge
                          key={effect.typeId}
                          label={effect.name}
                          selected={isSelected}
                          onPress={() => {
                            if (slot && !isSelected) {
                              setSlotType(slot.slotIndex, effect.typeId);
                            }
                          }}
                        />
                      );
                    })}
                  </WrapStack>
                )}
              </VStack>
            )}

            {panelTab === "routing" && (
              <VStack style={styles.overlayPanel}>
                <Text style={styles.todoText}>Routing (todo).</Text>
              </VStack>
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
  todoText: {
    fontSize: 14,
    color: "#666",
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
