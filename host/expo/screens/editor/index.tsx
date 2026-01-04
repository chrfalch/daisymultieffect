import React from "react";
import { View, Text, StyleSheet, ScrollView, Pressable } from "react-native";
import { GestureHandlerRootView, Switch } from "react-native-gesture-handler";
import Animated, { SlideInDown, SlideOutDown } from "react-native-reanimated";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Slider } from "../../components/Slider";
import { Card } from "../../components/Card";
import { Button } from "../../components/Button";
import { PedalSlot } from "../../components/PedalSlot";

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
  } = useDaisyMultiFX();
  const [expandedSlot, setExpandedSlot] = React.useState<number>(0);

  const selectSlot = (slotIndex: number) => {
    setExpandedSlot(slotIndex);
  };
  const slot =
    patch?.slots.find((s) => s.slotIndex === expandedSlot) ??
    patch?.slots[0] ??
    null;

  return (
    <GestureHandlerRootView style={styles.flex}>
      <ScrollView style={styles.container}>
        {/* Connection Status */}
        <View style={styles.statusCard}>
          <Text style={styles.statusTitle}>Connection Status</Text>
          <View style={styles.statusRow}>
            <View
              style={[
                styles.statusIndicator,
                { backgroundColor: isConnected ? "#4CAF50" : "#F44336" },
              ]}
            />
            <Text style={styles.statusText}>
              {isConnected ? "Connected" : "Disconnected"}
            </Text>
          </View>
          {connectionStatus && (
            <Text style={styles.statusDetail}>{connectionStatus.status}</Text>
          )}
          <View style={styles.buttonRow}>
            <Button title="Refresh Patch" onPress={refreshPatch} />
            <Button title="Refresh Effects" onPress={refreshEffectMeta} />
          </View>
        </View>

        {/* Available Effects */}
        {effectMeta.length > 0 && (
          <Card>
            <Text style={styles.sectionTitle}>Available Effects</Text>
            <View style={styles.effectsList}>
              {effectMeta.map((effect) => {
                const isSelected = slot?.typeId === effect.typeId;
                return (
                  <Pressable
                    key={effect.typeId}
                    style={[
                      styles.effectBadge,
                      isSelected && styles.effectBadgeSelected,
                    ]}
                    onPress={() => {
                      if (slot && !isSelected) {
                        setSlotType(slot.slotIndex, effect.typeId);
                      }
                    }}
                  >
                    <Text
                      style={[
                        styles.effectBadgeText,
                        isSelected && styles.effectBadgeTextSelected,
                      ]}
                    >
                      {effect.name}
                    </Text>
                  </Pressable>
                );
              })}
            </View>
          </Card>
        )}

        {/* Slots */}
        <View style={styles.wrappingPanel}>
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
        </View>

        {/* Empty State */}
        {!patch && (
          <View style={styles.emptyState}>
            <Text style={styles.emptyStateText}>
              No patch data received yet.
            </Text>
            <Text style={styles.emptyStateHint}>
              Make sure the VST or hardware is running.
            </Text>
          </View>
        )}
      </ScrollView>

      {slot && Object.keys(slot.params).length > 0 && (
        <Animated.View
          style={styles.parameterPanel}
          entering={SlideInDown}
          exiting={SlideOutDown}
        >
          <View style={styles.parameterPanelHeader}>
            <Text style={styles.parameterPanelTitle}>
              {getEffectName(slot.typeId) || `Slot ${slot.slotIndex + 1}`}
            </Text>
            <Switch
              value={slot.enabled}
              onValueChange={(value) => setSlotEnabled(slot.slotIndex, value)}
            />
          </View>
          {/* Parameters */}
          <View style={styles.paramsContainer}>
            {Object.entries(slot.params).map(
              ([paramId, value]) =>
                getParamName(slot.typeId, Number(paramId)) && (
                  <Slider
                    key={paramId}
                    label={getParamName(slot.typeId, Number(paramId))}
                    value={value}
                    min={0}
                    max={127}
                    onValueChangeEnd={(newValue) =>
                      setSlotParam(slot.slotIndex, Number(paramId), newValue)
                    }
                  />
                )
            )}
          </View>
        </Animated.View>
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
  statusCard: {
    backgroundColor: "#fff",
    margin: 16,
    padding: 16,
    borderRadius: 12,
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  wrappingPanel: {
    flexDirection: "row",
    flexWrap: "wrap",
    justifyContent: "space-between",
    gap: 16,
  },
  parameterPanel: {
    padding: 16,
  },
  statusTitle: {
    fontSize: 18,
    fontWeight: "600",
    marginBottom: 12,
  },
  statusRow: {
    flexDirection: "row",
    alignItems: "center",
    marginBottom: 8,
  },
  statusIndicator: {
    width: 12,
    height: 12,
    borderRadius: 6,
    marginRight: 8,
  },
  statusText: {
    fontSize: 16,
    fontWeight: "500",
  },
  statusDetail: {
    fontSize: 14,
    color: "#666",
    marginBottom: 12,
  },
  buttonRow: {
    flexDirection: "row",
    gap: 12,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: "600",
    marginBottom: 12,
  },
  effectsList: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: 8,
  },
  effectBadge: {
    backgroundColor: "#E3F2FD",
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
  },
  effectBadgeSelected: {
    backgroundColor: "#2196F3",
  },
  effectBadgeText: {
    color: "#1976D2",
    fontSize: 14,
  },
  effectBadgeTextSelected: {
    color: "#fff",
    fontWeight: "600",
  },
  parameterPanelHeader: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 12,
  },
  parameterPanelTitle: {
    fontSize: 18,
    fontWeight: "600",
  },
  paramsContainer: {
    marginTop: 12,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: "#eee",
  },
  emptyState: {
    margin: 16,
    padding: 32,
    alignItems: "center",
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
