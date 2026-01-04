import React from "react";
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Pressable,
} from "react-native";
import { GestureHandlerRootView, Switch } from "react-native-gesture-handler";
import Animated, { SlideInDown, SlideOutDown } from "react-native-reanimated";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Slider } from "../../components/Slider";

export const EditorScreen: React.FC = () => {
  const {
    isConnected,
    connectionStatus,
    patch,
    effectMeta,
    refreshPatch,
    refreshEffectMeta,
    setSlotEnabled,
    setSlotParam,
    getEffectName,
    getParamName,
  } = useDaisyMultiFX();
  const [expandedSlot, setExpandedSlot] = React.useState<number | null>(null);

  const toggleSlot = (slotIndex: number) => {
    setExpandedSlot((prev) => (prev === slotIndex ? null : slotIndex));
  };
  const slot = patch?.slots.find((s) => s.slotIndex === expandedSlot) ?? null;

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
            <TouchableOpacity style={styles.button} onPress={refreshPatch}>
              <Text style={styles.buttonText}>Refresh Patch</Text>
            </TouchableOpacity>
            <TouchableOpacity style={styles.button} onPress={refreshEffectMeta}>
              <Text style={styles.buttonText}>Refresh Effects</Text>
            </TouchableOpacity>
          </View>
        </View>

        {/* Available Effects */}
        {effectMeta.length > 0 && (
          <View style={styles.card}>
            <Text style={styles.sectionTitle}>Available Effects</Text>
            <View style={styles.effectsList}>
              {effectMeta.map((effect) => (
                <View key={effect.typeId} style={styles.effectBadge}>
                  <Text style={styles.effectBadgeText}>{effect.name}</Text>
                </View>
              ))}
            </View>
          </View>
        )}

        {/* Slots */}
        <View style={styles.wrappingPanel}>
          {patch?.slots.map((current) => (
            <Pressable
              key={current.slotIndex}
              style={[
                styles.slotTitleContainer,
                current === slot && { backgroundColor: "#2196F3" },
              ]}
              onPress={() => toggleSlot(current.slotIndex)}
              accessibilityRole="button"
              accessibilityLabel={`Toggle ${getEffectName(
                current.typeId
              )} details`}
            >
              <View style={styles.card}>
                <View style={styles.slotHeader}>
                  <Text style={styles.slotTitle}>
                    {getEffectName(current.typeId) ||
                      `Slot ${current.slotIndex + 1}`}
                  </Text>
                  <Pressable
                    onPress={() =>
                      setSlotEnabled(current.slotIndex, !current.enabled)
                    }
                  >
                    {({ pressed }) => (
                      <View
                        style={[
                          styles.enabledBadge,
                          {
                            backgroundColor: current.enabled
                              ? "#4CAF50"
                              : "#9E9E9E",
                          },
                          pressed && { opacity: 0.7 },
                        ]}
                      >
                        <Text style={styles.enabledText}>
                          {current.enabled ? "ON" : "OFF"}
                        </Text>
                      </View>
                    )}
                  </Pressable>
                </View>
              </View>
            </Pressable>
          ))}
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
          <Pressable onPress={() => toggleSlot(slot.slotIndex)}>
            <View style={styles.slotHeader}>
              <Text style={styles.slotTitle}>
                {getEffectName(slot.typeId) || `Slot ${slot.slotIndex + 1}`}
              </Text>
              <Switch
                value={slot.enabled}
                onValueChange={(value) => setSlotEnabled(slot.slotIndex, value)}
              />
            </View>
          </Pressable>
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
  button: {
    backgroundColor: "#2196F3",
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  buttonText: {
    color: "#fff",
    fontWeight: "500",
  },
  card: {
    backgroundColor: "#fff",
    margin: 16,
    marginTop: 0,
    padding: 16,
    borderRadius: 12,
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
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
  effectBadgeText: {
    color: "#1976D2",
    fontSize: 14,
  },
  slotHeader: {
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 12,
    width: 70,
    height: 80,
  },
  slotTitleContainer: {
    flexDirection: "row",
    alignItems: "center",
  },
  slotTitle: {
    fontSize: 18,
    fontWeight: "600",
    textAlign: "center",
  },
  enabledBadge: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
  },
  enabledText: {
    color: "#fff",
    fontSize: 12,
    fontWeight: "600",
  },
  infoRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    marginBottom: 4,
  },
  infoLabel: {
    fontSize: 14,
    color: "#666",
  },
  infoValue: {
    fontSize: 14,
    fontWeight: "500",
  },
  paramsContainer: {
    marginTop: 12,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: "#eee",
  },
  paramsTitle: {
    fontSize: 14,
    fontWeight: "600",
    color: "#666",
    marginBottom: 8,
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
