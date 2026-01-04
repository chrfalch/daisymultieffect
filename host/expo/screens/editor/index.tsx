import React from "react";
import { View, Text, StyleSheet, ScrollView } from "react-native";
import { GestureHandlerRootView, Switch } from "react-native-gesture-handler";
import Animated, { SlideInDown, SlideOutDown } from "react-native-reanimated";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Slider } from "../../components/Slider";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { PedalSlot } from "../../components/PedalSlot";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { Button } from "../../components/Button";
import { Badge } from "../../components/Badge";
import { HStack, VStack, WrapStack } from "../../components/Stack";

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

        {/* Available Effects */}
        {effectMeta.length > 0 && (
          <Card>
            <CardTitle>Available Effects</CardTitle>
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
          </Card>
        )}

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

      {slot && Object.keys(slot.params).length > 0 && (
        <VStack padding={16}>
          <HStack justify="space-between" align="center">
            <Text style={styles.parameterPanelTitle}>
              {getEffectName(slot.typeId) || `Slot ${slot.slotIndex + 1}`}
            </Text>
            <Switch
              value={slot.enabled}
              onValueChange={(value) => setSlotEnabled(slot.slotIndex, value)}
            />
          </HStack>
          {/* Parameters */}
          <VStack style={styles.paramsContainer}>
            {Object.entries(slot.params).map(
              ([paramId, value]) =>
                getParamName(slot.typeId, Number(paramId)) && (
                  <Slider
                    key={paramId}
                    label={getParamName(slot.typeId, Number(paramId))}
                    value={value}
                    min={0}
                    max={127}
                    onValueChange={(newValue) =>
                      setSlotParam(slot.slotIndex, Number(paramId), newValue)
                    }
                  />
                )
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
  paramsContainer: {
    marginTop: 12,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: "#eee",
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
