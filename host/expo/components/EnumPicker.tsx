import React from "react";
import {
  Text,
  StyleSheet,
  Pressable,
  Modal,
  FlatList,
  View,
} from "react-native";
import Ionicons from "@expo/vector-icons/Ionicons";
import { VStack, HStack } from "./Stack";
import { useThemeColors } from "./ThemeProvider";

interface EnumOption {
  value: number;
  name: string;
}

interface EnumPickerProps {
  label: string;
  description?: string;
  value: number;
  options: EnumOption[];
  onValueChange: (value: number) => void;
}

export const EnumPicker: React.FC<EnumPickerProps> = ({
  label,
  description,
  value,
  options,
  onValueChange,
}) => {
  const colors = useThemeColors();
  const [isOpen, setIsOpen] = React.useState(false);

  const selectedOption = options.find((opt) => opt.value === value);
  const displayValue = selectedOption?.name ?? `Unknown (${value})`;

  const handleSelect = (optionValue: number) => {
    onValueChange(optionValue);
    setIsOpen(false);
  };

  return (
    <VStack gap={4}>
      <HStack justify="space-between" align="center">
        <Text style={[styles.label, { color: colors.text }]}>{label}</Text>
        <Pressable
          onPress={() => setIsOpen(true)}
          style={({ pressed }) => [
            styles.pickerButton,
            { backgroundColor: colors.surfaceSecondary, borderColor: colors.border },
            pressed && styles.pickerButtonPressed,
          ]}
        >
          <HStack gap={8} align="center">
            <Text style={[styles.pickerValue, { color: colors.accentDark }]}>
              {displayValue}
            </Text>
            <Ionicons name="chevron-down" size={16} color={colors.accentDark} />
          </HStack>
        </Pressable>
      </HStack>
      {description && (
        <Text style={[styles.description, { color: colors.textSecondary }]}>
          {description}
        </Text>
      )}

      <Modal
        visible={isOpen}
        transparent
        animationType="fade"
        onRequestClose={() => setIsOpen(false)}
      >
        <Pressable style={styles.modalOverlay} onPress={() => setIsOpen(false)}>
          <View
            style={[
              styles.modalContent,
              { backgroundColor: colors.surfacePrimary },
            ]}
          >
            <Text style={[styles.modalTitle, { color: colors.text, borderBottomColor: colors.borderLight }]}>
              {label}
            </Text>
            <FlatList
              data={options}
              keyExtractor={(item) => String(item.value)}
              renderItem={({ item }) => (
                <Pressable
                  onPress={() => handleSelect(item.value)}
                  style={({ pressed }) => [
                    styles.optionItem,
                    item.value === value && { backgroundColor: colors.accentLight },
                    pressed && { backgroundColor: colors.surfaceSecondary },
                  ]}
                >
                  <HStack justify="space-between" align="center">
                    <Text
                      style={[
                        styles.optionText,
                        { color: colors.text },
                        item.value === value && { color: colors.accentDark, fontWeight: "500" },
                      ]}
                    >
                      {item.name}
                    </Text>
                    {item.value === value && (
                      <Ionicons name="checkmark" size={20} color={colors.accentDark} />
                    )}
                  </HStack>
                </Pressable>
              )}
              ItemSeparatorComponent={() => (
                <View style={[styles.separator, { backgroundColor: colors.borderLight }]} />
              )}
            />
          </View>
        </Pressable>
      </Modal>
    </VStack>
  );
};

const styles = StyleSheet.create({
  label: {
    fontSize: 14,
    fontWeight: "500",
  },
  description: {
    fontSize: 12,
    marginTop: 2,
  },
  pickerButton: {
    paddingVertical: 8,
    paddingHorizontal: 12,
    borderRadius: 8,
    borderCurve: "continuous",
    borderWidth: 1,
  },
  pickerButtonPressed: {
    opacity: 0.7,
  },
  pickerValue: {
    fontSize: 14,
    fontWeight: "500",
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: "rgba(0,0,0,0.5)",
    justifyContent: "center",
    alignItems: "center",
    padding: 20,
  },
  modalContent: {
    borderRadius: 12,
    borderCurve: "continuous",
    width: "100%",
    maxWidth: 320,
    maxHeight: "60%",
    overflow: "hidden",
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: "600",
    padding: 16,
    borderBottomWidth: 1,
    textAlign: "center",
  },
  optionItem: {
    paddingVertical: 14,
    paddingHorizontal: 16,
  },
  optionText: {
    fontSize: 16,
  },
  separator: {
    height: 1,
    marginHorizontal: 16,
  },
});
