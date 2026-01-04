import React from "react";
import { ScrollView, Text, StyleSheet, View } from "react-native";
import { Button as SwiftUIButton, ContextMenu, Host } from "@expo/ui/swift-ui";
import { disabled } from "@expo/ui/swift-ui/modifiers";
import type { EffectSlot, Patch } from "../../modules/daisy-multi-fx";
import { Badge } from "../../components/Badge";
import { Slider } from "../../components/Slider";
import { HStack, VStack, WrapStack } from "../../components/Stack";

export const RoutingPanel: React.FC<{
  patch: Patch;
  slot: EffectSlot;
  getEffectShortName: (typeId: number) => string;
  setSlotRouting: (slot: number, inputL: number, inputR: number) => void;
  setSlotSumToMono: (slot: number, sumToMono: boolean) => void;
  setSlotMix: (slot: number, dry: number, wet: number) => void;
  setSlotChannelPolicy: (slot: number, channelPolicy: number) => void;
}> = ({
  patch,
  slot,
  getEffectShortName,
  setSlotRouting,
  setSlotSumToMono,
  setSlotMix,
  setSlotChannelPolicy,
}) => {
  const routeOptions = React.useMemo(() => {
    const options: Array<{ label: string; value: number; enabled: boolean }> = [
      { label: "IN", value: 255, enabled: true },
    ];

    // Avoid forward references (future slot outputs are 0 for the current frame)
    for (let i = 0; i < 12; i++) {
      const enabled = i < slot.slotIndex;
      const s = patch.slots[i];
      const short = s ? getEffectShortName(s.typeId) : `Slot ${i + 1}`;
      options.push({ label: `${i + 1}:${short}`, value: i, enabled });
    }

    return options;
  }, [patch, slot.slotIndex, getEffectShortName]);

  const policyOptions = React.useMemo(
    () =>
      [
        { label: "Auto", value: 0 },
        { label: "Mono", value: 1 },
        { label: "Stereo", value: 2 },
      ] as const,
    []
  );

  const getRouteLabel = React.useCallback(
    (value: number) => {
      return (
        routeOptions.find((o) => o.value === value)?.label ?? String(value)
      );
    },
    [routeOptions]
  );

  return (
    <ScrollView style={styles.overlayPanel}>
      <VStack paddingVertical={16} gap={12}>
        <Text style={styles.routingSectionTitle}>Inputs</Text>

        <HStack gap={12} align="flex-start" style={styles.inputsRow}>
          <VStack gap={12} style={styles.inputsColumn}>
            <VStack gap={8}>
              <Text style={styles.routingLabel}>Left input</Text>
              <Host matchContents={{ horizontal: true, vertical: true }}>
                <ContextMenu activationMethod="singlePress">
                  <ContextMenu.Items>
                    {routeOptions.map((opt) => (
                      <SwiftUIButton
                        key={`inL-${opt.value}`}
                        label={opt.label}
                        systemImage={
                          slot.inputL === opt.value ? "checkmark" : undefined
                        }
                        modifiers={[disabled(!opt.enabled)]}
                        onPress={
                          opt.enabled
                            ? () =>
                                setSlotRouting(
                                  slot.slotIndex,
                                  opt.value,
                                  slot.inputR
                                )
                            : undefined
                        }
                      />
                    ))}
                  </ContextMenu.Items>

                  <ContextMenu.Trigger>
                    <View
                      accessibilityRole="button"
                      accessibilityLabel="Left input"
                      style={styles.menuTrigger}
                    >
                      <Text style={styles.menuTriggerText}>
                        {getRouteLabel(slot.inputL)}
                      </Text>
                    </View>
                  </ContextMenu.Trigger>
                </ContextMenu>
              </Host>
            </VStack>

            <VStack gap={8}>
              <Text style={styles.routingLabel}>Right input</Text>
              <Host matchContents={{ horizontal: true, vertical: true }}>
                <ContextMenu activationMethod="singlePress">
                  <ContextMenu.Items>
                    {routeOptions.map((opt) => (
                      <SwiftUIButton
                        key={`inR-${opt.value}`}
                        label={opt.label}
                        systemImage={
                          slot.inputR === opt.value ? "checkmark" : undefined
                        }
                        modifiers={[disabled(!opt.enabled)]}
                        onPress={
                          opt.enabled
                            ? () =>
                                setSlotRouting(
                                  slot.slotIndex,
                                  slot.inputL,
                                  opt.value
                                )
                            : undefined
                        }
                      />
                    ))}
                  </ContextMenu.Items>

                  <ContextMenu.Trigger>
                    <View
                      accessibilityRole="button"
                      accessibilityLabel="Right input"
                      style={styles.menuTrigger}
                    >
                      <Text style={styles.menuTriggerText}>
                        {getRouteLabel(slot.inputR)}
                      </Text>
                    </View>
                  </ContextMenu.Trigger>
                </ContextMenu>
              </Host>
            </VStack>
          </VStack>

          <VStack gap={8} style={styles.inputsColumn}>
            <Text style={styles.routingLabel}>Channel</Text>
            <WrapStack gap={8}>
              <Badge
                label={`Sum: ${slot.sumToMono ? "ON" : "OFF"}`}
                selected={slot.sumToMono}
                onPress={() =>
                  setSlotSumToMono(slot.slotIndex, !slot.sumToMono)
                }
              />
            </WrapStack>

            <Text style={styles.routingLabel}>Policy</Text>
            <WrapStack gap={8}>
              {policyOptions.map((p) => (
                <Badge
                  key={`pol-${p.value}`}
                  label={p.label}
                  selected={slot.channelPolicy === p.value}
                  onPress={() => setSlotChannelPolicy(slot.slotIndex, p.value)}
                />
              ))}
            </WrapStack>
          </VStack>
        </HStack>

        <Text style={styles.routingSectionTitle}>Mix</Text>
        <VStack gap={12}>
          <Slider
            label="Dry"
            description="Unprocessed (clean) signal level — 0=mute, 127=max"
            value={slot.dry}
            min={0}
            max={127}
            formatValue={(raw) => String(raw)}
            onValueChange={(dry) => setSlotMix(slot.slotIndex, dry, slot.wet)}
          />
          <Slider
            label="Wet"
            description="Processed (effect) signal level — 0=mute, 127=max"
            value={slot.wet}
            min={0}
            max={127}
            formatValue={(raw) => String(raw)}
            onValueChange={(wet) => setSlotMix(slot.slotIndex, slot.dry, wet)}
          />
        </VStack>
      </VStack>
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  overlayPanel: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
  },
  inputsRow: {
    flexWrap: "wrap",
  },
  inputsColumn: {
    minWidth: 150,
  },
  menuTrigger: {
    backgroundColor: "#E3F2FD",
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    alignSelf: "flex-start",
  },
  menuTriggerText: {
    color: "#1976D2",
    fontSize: 14,
  },
  routingSectionTitle: {
    fontSize: 14,
    fontWeight: "600",
    color: "#333",
  },
  routingLabel: {
    fontSize: 13,
    color: "#666",
  },
});
