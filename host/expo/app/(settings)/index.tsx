import React from "react";
import { Text, ScrollView, View } from "react-native";
import { useDaisyMultiFX } from "../../hooks/useDaisyMultiFX";
import { Card } from "../../components/Card";
import { CardTitle } from "../../components/CardTitle";
import { ConnectionStatus } from "../../components/ConnectionStatus";
import { DeviceStatusMeter } from "../../components/DeviceStatusMeter";
import { GainControl } from "../../components/GainControl";
import { Button } from "../../components/Button";
import { HStack, VStack } from "../../components/Stack";
import { useThemeColors } from "../../components/ThemeProvider";

export default function SettingsScreen() {
  const colors = useThemeColors();
  const {
    isConnected,
    connectionStatus,
    patch,
    refreshPatch,
    refreshEffectMeta,
    pushPatchToVst,
    setGlobalBypass,
    setInputGain,
    setOutputGain,
  } = useDaisyMultiFX();

  const [globalBypass, setGlobalBypassState] = React.useState(false);

  const handleGlobalBypassChange = React.useCallback(
    (bypass: boolean) => {
      setGlobalBypassState(bypass);
      setGlobalBypass(bypass);
    },
    [setGlobalBypass],
  );

  return (
    <ScrollView
      style={{ flex: 1, backgroundColor: colors.background }}
      contentContainerStyle={{ padding: 16, gap: 16, paddingBottom: 32 }}
      contentInsetAdjustmentBehavior="automatic"
    >
      {/* Connection Section */}
      <Card>
        <VStack gap={12}>
          <CardTitle>Connection</CardTitle>
          <ConnectionStatus
            isConnected={isConnected}
            connectionStatus={connectionStatus}
          />
          <HStack gap={8}>
            <Button title="Load Patch from VST" onPress={refreshPatch} />
            <Button title="Push Patch to VST" onPress={pushPatchToVst} />
          </HStack>
          <Button title="Refresh Effect Metadata" onPress={refreshEffectMeta} />
        </VStack>
      </Card>

      {/* Device Status Section */}
      <Card>
        <VStack gap={12}>
          <CardTitle>Device Status</CardTitle>
          <DeviceStatusMeter />
        </VStack>
      </Card>

      {/* Gain Controls Section */}
      {patch && (
        <Card>
          <VStack gap={12}>
            <CardTitle>Gain Controls</CardTitle>
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

      {/* About Section */}
      <Card>
        <VStack gap={8}>
          <CardTitle>About</CardTitle>
          <Text style={{ color: colors.textSecondary, fontSize: 14 }}>
            DaisyMultiEffect Controller
          </Text>
          <Text style={{ color: colors.textTertiary, fontSize: 12 }}>
            A modular multi-effect guitar pedal platform using Daisy Seed.
          </Text>
          <Text style={{ color: colors.textTertiary, fontSize: 12 }}>
            Version 1.0.0
          </Text>
        </VStack>
      </Card>
    </ScrollView>
  );
}
