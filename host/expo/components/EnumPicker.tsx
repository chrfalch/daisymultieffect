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
        <Text style={styles.label}>{label}</Text>
        <Pressable
          onPress={() => setIsOpen(true)}
          style={({ pressed }) => [
            styles.pickerButton,
            pressed && styles.pickerButtonPressed,
          ]}
        >
          <HStack gap={8} align="center">
            <Text style={styles.pickerValue}>{displayValue}</Text>
            <Ionicons name="chevron-down" size={16} color="#1976D2" />
          </HStack>
        </Pressable>
      </HStack>
      {description && <Text style={styles.description}>{description}</Text>}

      <Modal
        visible={isOpen}
        transparent
        animationType="fade"
        onRequestClose={() => setIsOpen(false)}
      >
        <Pressable style={styles.modalOverlay} onPress={() => setIsOpen(false)}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>{label}</Text>
            <FlatList
              data={options}
              keyExtractor={(item) => String(item.value)}
              renderItem={({ item }) => (
                <Pressable
                  onPress={() => handleSelect(item.value)}
                  style={({ pressed }) => [
                    styles.optionItem,
                    item.value === value && styles.optionItemSelected,
                    pressed && styles.optionItemPressed,
                  ]}
                >
                  <HStack justify="space-between" align="center">
                    <Text
                      style={[
                        styles.optionText,
                        item.value === value && styles.optionTextSelected,
                      ]}
                    >
                      {item.name}
                    </Text>
                    {item.value === value && (
                      <Ionicons name="checkmark" size={20} color="#1976D2" />
                    )}
                  </HStack>
                </Pressable>
              )}
              ItemSeparatorComponent={() => <View style={styles.separator} />}
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
    color: "#333",
  },
  description: {
    fontSize: 12,
    color: "#666",
    marginTop: 2,
  },
  pickerButton: {
    paddingVertical: 8,
    paddingHorizontal: 12,
    backgroundColor: "#f0f0f0",
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#ddd",
  },
  pickerButtonPressed: {
    backgroundColor: "#e0e0e0",
  },
  pickerValue: {
    fontSize: 14,
    color: "#1976D2",
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
    backgroundColor: "#fff",
    borderRadius: 12,
    width: "100%",
    maxWidth: 320,
    maxHeight: "60%",
    overflow: "hidden",
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: "600",
    color: "#333",
    padding: 16,
    borderBottomWidth: 1,
    borderBottomColor: "#eee",
    textAlign: "center",
  },
  optionItem: {
    paddingVertical: 14,
    paddingHorizontal: 16,
  },
  optionItemSelected: {
    backgroundColor: "#e3f2fd",
  },
  optionItemPressed: {
    backgroundColor: "#f5f5f5",
  },
  optionText: {
    fontSize: 16,
    color: "#333",
  },
  optionTextSelected: {
    color: "#1976D2",
    fontWeight: "500",
  },
  separator: {
    height: 1,
    backgroundColor: "#eee",
    marginHorizontal: 16,
  },
});
