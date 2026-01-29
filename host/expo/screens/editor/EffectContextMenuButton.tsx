import React from "react";
import { StyleSheet, Text, View } from "react-native";
import Ionicons from "@expo/vector-icons/Ionicons";
import { Button as SwiftUIButton, ContextMenu, Host } from "@expo/ui/swift-ui";
import type { EffectMeta, EffectSlot } from "../../modules/daisy-multi-fx";

export const EffectContextMenuButton: React.FC<{
  slot: EffectSlot;
  effectMeta: EffectMeta[];
  setSlotType: (slot: number, typeId: number) => void;
}> = ({ slot, effectMeta, setSlotType }) => {
  const currentEffectName = React.useMemo(() => {
    if (slot.typeId === 0) return "Off";
    return effectMeta.find((e) => e.typeId === slot.typeId)?.name ?? "Unknown";
  }, [effectMeta, slot.typeId]);

  // Filter out "Off" from effectMeta since we add it explicitly, and sort reverse alphabetically
  const effectsToShow = React.useMemo(
    () =>
      effectMeta
        .filter((e) => e.typeId !== 0)
        .sort((a, b) => b.name.localeCompare(a.name)),
    [effectMeta],
  );

  return (
    <Host matchContents={{ horizontal: true, vertical: true }}>
      <ContextMenu activationMethod="singlePress">
        <ContextMenu.Items>
          {/* Always show Off option at the top */}
          <SwiftUIButton
            label="Off"
            onPress={() => {
              if (slot.typeId !== 0) {
                setSlotType(slot.slotIndex, 0);
              }
            }}
          />
          {effectsToShow.length === 0 ? (
            <SwiftUIButton label="No effects loaded" />
          ) : (
            effectsToShow.map((effect) => (
              <SwiftUIButton
                key={effect.typeId}
                label={effect.name}
                onPress={() => {
                  if (slot.typeId !== effect.typeId) {
                    setSlotType(slot.slotIndex, effect.typeId);
                  }
                }}
              />
            ))
          )}
        </ContextMenu.Items>

        <ContextMenu.Trigger>
          <View
            accessibilityRole="button"
            accessibilityLabel="Select effect"
            style={styles.triggerButton}
          >
            <Ionicons name="sparkles-outline" size={16} color="#1976D2" />
            <Text numberOfLines={1} style={styles.triggerText}>
              {currentEffectName}
            </Text>
          </View>
        </ContextMenu.Trigger>
      </ContextMenu>
    </Host>
  );
};

const styles = StyleSheet.create({
  triggerButton: {
    backgroundColor: "#E3F2FD",
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    gap: 6,
  },
  triggerText: {
    color: "#1976D2",
    fontSize: 14,
    maxWidth: 220,
  },
});
