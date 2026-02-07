import React from "react";
import { View, Text, StyleSheet } from "react-native";

type ConnectionStatusValue = {
  status: string;
};

type ConnectionStatusProps = {
  isConnected: boolean;
  connectionStatus?: ConnectionStatusValue | null;
};

export const ConnectionStatus: React.FC<ConnectionStatusProps> = ({
  isConnected,
  connectionStatus,
}) => {
  return (
    <>
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
    </>
  );
};

const styles = StyleSheet.create({
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
});
