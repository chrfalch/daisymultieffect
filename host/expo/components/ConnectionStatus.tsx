import React from "react";
import { View, Text, StyleSheet } from "react-native";
import { useThemeColors } from "./ThemeProvider";

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
  const colors = useThemeColors();

  return (
    <>
      <View style={styles.statusRow}>
        <View
          style={[
            styles.statusIndicator,
            { backgroundColor: isConnected ? colors.success : colors.error },
          ]}
        />
        <Text style={[styles.statusText, { color: colors.text }]}>
          {isConnected ? "Connected" : "Disconnected"}
        </Text>
      </View>
      {connectionStatus && (
        <Text style={[styles.statusDetail, { color: colors.textSecondary }]}>
          {connectionStatus.status}
        </Text>
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
