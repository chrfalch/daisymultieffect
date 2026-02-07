import React from "react";
import { View, Text } from "react-native";
import { useColors } from "../hooks/useColors";

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
  const colors = useColors();
  return (
    <>
      <View style={{ flexDirection: "row", alignItems: "center", marginBottom: 8 }}>
        <View
          style={{
            width: 12,
            height: 12,
            borderRadius: 6,
            marginRight: 8,
            backgroundColor: isConnected ? colors.success : colors.danger,
          }}
        />
        <Text style={{ fontSize: 16, fontWeight: "500", color: colors.text }}>
          {isConnected ? "Connected" : "Disconnected"}
        </Text>
      </View>
      {connectionStatus && (
        <Text style={{ fontSize: 14, color: colors.textSecondary, marginBottom: 12 }}>
          {connectionStatus.status}
        </Text>
      )}
    </>
  );
};
