import React from "react";
import { ScrollView, Text, View } from "react-native";
import { Button as SwiftUIButton, ContextMenu, Host } from "@expo/ui/swift-ui";
import { disabled } from "@expo/ui/swift-ui/modifiers";
import type { EffectSlot, Patch } from "../../modules/daisy-multi-fx";
import { Badge } from "../../components/Badge";
import { Slider } from "../../components/Slider";
import { HStack, VStack, WrapStack } from "../../components/Stack";
import { useColors } from "../../hooks/useColors";

const InputSelector: React.FC<{
  label: string;
  value: number;
  options: Array<{ label: string; value: number; enabled: boolean }>;
  getRouteLabel: (value: number) => string;
  onChange: (value: number) => void;
}> = ({ label, value, options, getRouteLabel, onChange }) => {
  const colors = useColors();
  return (
    <VStack gap={4} style={{ minWidth: 80 }}>
      <Text style={{ fontSize: 12, color: colors.textTertiary }}>{label}</Text>
      <Host matchContents={{ horizontal: true, vertical: true }}>
        <ContextMenu activationMethod="singlePress">
          <ContextMenu.Items>
            {options.map((opt) => (
              <SwiftUIButton
                key={`${label}-${opt.value}`}
                label={opt.label}
                systemImage={value === opt.value ? "checkmark" : undefined}
                modifiers={[disabled(!opt.enabled)]}
                onPress={opt.enabled ? () => onChange(opt.value) : undefined}
              />
            ))}
          </ContextMenu.Items>
          <ContextMenu.Trigger>
            <View
              accessibilityRole="button"
              accessibilityLabel={label}
              style={{
                backgroundColor: colors.primaryTint,
                paddingHorizontal: 14,
                paddingVertical: 8,
                borderRadius: 16,
                alignSelf: "flex-start",
              }}
            >
              <Text style={{ color: colors.primaryOnTint, fontSize: 14, fontWeight: "500" }}>
                {getRouteLabel(value)}
              </Text>
            </View>
          </ContextMenu.Trigger>
        </ContextMenu>
      </Host>
    </VStack>
  );
};

export const RoutingPanel: React.FC<{
  patch: Patch;
  slot: EffectSlot;
  getEffectName: (typeId: number) => string;
  setSlotRouting: (slot: number, inputL: number, inputR: number) => void;
  setSlotSumToMono: (slot: number, sumToMono: boolean) => void;
  setSlotMix: (slot: number, dry: number, wet: number) => void;
  setSlotChannelPolicy: (slot: number, channelPolicy: number) => void;
}> = ({
  patch,
  slot,
  getEffectName,
  setSlotRouting,
  setSlotSumToMono,
  setSlotMix,
  setSlotChannelPolicy,
}) => {
  const colors = useColors();
  const routeOptions = React.useMemo(() => {
    const options: Array<{ label: string; value: number; enabled: boolean }> = [
      { label: "IN", value: 255, enabled: true },
    ];

    for (let i = 0; i < 12; i++) {
      const enabled = i < slot.slotIndex;
      const s = patch.slots[i];
      const short = s ? getEffectName(s.typeId) : `Slot ${i + 1}`;
      options.push({ label: `${i + 1}:${short}`, value: i, enabled });
    }

    return options;
  }, [patch, slot.slotIndex, getEffectName]);

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
    (value: number) =>
      routeOptions.find((o) => o.value === value)?.label ?? String(value),
    [routeOptions]
  );

  return (
    <ScrollView style={{ position: "absolute", top: 0, left: 0, right: 0, bottom: 0 }}>
      <VStack paddingVertical={16} gap={16}>
        {/* Input Sources */}
        <View style={{ gap: 10 }}>
          <Text
            style={{
              fontSize: 13,
              fontWeight: "600",
              color: colors.text,
              textTransform: "uppercase",
              letterSpacing: 0.5,
            }}
          >
            Input
          </Text>
          <HStack gap={16} style={{ flexWrap: "wrap" }}>
            <InputSelector
              label="Left"
              value={slot.inputL}
              options={routeOptions}
              getRouteLabel={getRouteLabel}
              onChange={(v) => setSlotRouting(slot.slotIndex, v, slot.inputR)}
            />
            <InputSelector
              label="Right"
              value={slot.inputR}
              options={routeOptions}
              getRouteLabel={getRouteLabel}
              onChange={(v) => setSlotRouting(slot.slotIndex, slot.inputL, v)}
            />
          </HStack>
        </View>

        {/* Channel Processing */}
        <View style={{ gap: 10 }}>
          <Text
            style={{
              fontSize: 13,
              fontWeight: "600",
              color: colors.text,
              textTransform: "uppercase",
              letterSpacing: 0.5,
            }}
          >
            Channel
          </Text>
          <HStack gap={16} align="flex-start">
            <VStack gap={4}>
              <Text style={{ fontSize: 12, color: colors.textTertiary }}>Sum to Mono</Text>
              <Text style={{ fontSize: 11, color: colors.textDisabled, marginBottom: 2 }}>
                Mix L+R inputs before processing
              </Text>
              <Badge
                label={slot.sumToMono ? "On" : "Off"}
                selected={slot.sumToMono}
                onPress={() =>
                  setSlotSumToMono(slot.slotIndex, !slot.sumToMono)
                }
              />
            </VStack>
            <VStack gap={4} style={{ flex: 1 }}>
              <Text style={{ fontSize: 12, color: colors.textTertiary }}>Processing</Text>
              <Text style={{ fontSize: 11, color: colors.textDisabled, marginBottom: 2 }}>
                Auto: effect decides · Mono: single channel · Stereo: both
              </Text>
              <HStack gap={6}>
                {policyOptions.map((p) => (
                  <Badge
                    key={`pol-${p.value}`}
                    label={p.label}
                    selected={slot.channelPolicy === p.value}
                    onPress={() =>
                      setSlotChannelPolicy(slot.slotIndex, p.value)
                    }
                  />
                ))}
              </HStack>
            </VStack>
          </HStack>
        </View>

        {/* Mix Controls */}
        <View style={{ gap: 10 }}>
          <Text
            style={{
              fontSize: 13,
              fontWeight: "600",
              color: colors.text,
              textTransform: "uppercase",
              letterSpacing: 0.5,
            }}
          >
            Mix
          </Text>
          <VStack gap={12}>
            <Slider
              label="Dry"
              description="Unprocessed (clean) signal level"
              value={slot.dry}
              min={0}
              max={127}
              formatValue={(raw) => String(raw)}
              onValueChange={(dry) => setSlotMix(slot.slotIndex, dry, slot.wet)}
            />
            <Slider
              label="Wet"
              description="Processed (effect) signal level"
              value={slot.wet}
              min={0}
              max={127}
              formatValue={(raw) => String(raw)}
              onValueChange={(wet) => setSlotMix(slot.slotIndex, slot.dry, wet)}
            />
          </VStack>
        </View>
      </VStack>
    </ScrollView>
  );
};
