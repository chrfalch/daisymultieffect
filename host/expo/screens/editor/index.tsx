import React from "react";
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Pressable,
} from "react-native";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";

export const EditorScreen: React.FC = () => {
  const {
    isConnected,
    connectionStatus,
    patch,
    effectMeta,
    refreshPatch,
    refreshEffectMeta,
    setSlotEnabled,
    getEffectName,
    getParamName,
  } = useDaisyMultiFX();

  return (
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
      {patch?.slots.map((slot) => (
        <View key={slot.slotIndex} style={styles.card}>
          <View style={styles.slotHeader}>
            <Text style={styles.slotTitle}>Slot {slot.slotIndex + 1}</Text>
            <Pressable
              onPress={() => setSlotEnabled(slot.slotIndex, !slot.enabled)}
            >
              {({ pressed }) => (
                <View
                  style={[
                    styles.enabledBadge,
                    { backgroundColor: slot.enabled ? "#4CAF50" : "#9E9E9E" },
                    pressed && { opacity: 0.7 },
                  ]}
                >
                  <Text style={styles.enabledText}>
                    {slot.enabled ? "ON" : "OFF"}
                  </Text>
                </View>
              )}
            </Pressable>
          </View>

          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Effect:</Text>
            <Text style={styles.infoValue}>{getEffectName(slot.typeId)}</Text>
          </View>

          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Dry/Wet:</Text>
            <Text style={styles.infoValue}>
              {slot.dry} / {slot.wet}
            </Text>
          </View>

          {/* Parameters */}
          {slot.typeId !== 0 && Object.keys(slot.params).length > 0 && (
            <View style={styles.paramsContainer}>
              <Text style={styles.paramsTitle}>Parameters</Text>
              {Object.entries(slot.params).map(([paramId, value]) => (
                <View key={paramId} style={styles.infoRow}>
                  <Text style={styles.infoLabel}>
                    {getParamName(slot.typeId, Number(paramId))}:
                  </Text>
                  <Text style={styles.infoValue}>{value}</Text>
                </View>
              ))}
            </View>
          )}
        </View>
      ))}

      {/* Empty State */}
      {!patch && (
        <View style={styles.emptyState}>
          <Text style={styles.emptyStateText}>No patch data received yet.</Text>
          <Text style={styles.emptyStateHint}>
            Make sure the VST or hardware is running.
          </Text>
        </View>
      )}
    </ScrollView>
  );
};

const styles = StyleSheet.create({
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
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 12,
  },
  slotTitle: {
    fontSize: 18,
    fontWeight: "600",
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
