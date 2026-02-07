import React from "react";
import {
  Text,
  Pressable,
  Modal,
  FlatList,
  View,
} from "react-native";
import Ionicons from "@expo/vector-icons/Ionicons";
import { VStack, HStack } from "./Stack";
import { useColors } from "../hooks/useColors";

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
  const colors = useColors();
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
        <Text style={{ fontSize: 14, fontWeight: "500", color: colors.text }}>
          {label}
        </Text>
        <Pressable
          onPress={() => setIsOpen(true)}
          style={({ pressed }) => ({
            paddingVertical: 8,
            paddingHorizontal: 12,
            backgroundColor: colors.surfaceRecessed,
            borderRadius: 8,
            borderWidth: 1,
            borderColor: colors.separator,
            opacity: pressed ? 0.7 : 1,
          })}
        >
          <HStack gap={8} align="center">
            <Text style={{ fontSize: 14, color: colors.primaryOnTint, fontWeight: "500" }}>
              {displayValue}
            </Text>
            <Ionicons name="chevron-down" size={16} color={colors.primaryOnTint} />
          </HStack>
        </Pressable>
      </HStack>
      {description && (
        <Text style={{ fontSize: 12, color: colors.textSecondary, marginTop: 2 }}>
          {description}
        </Text>
      )}

      <Modal
        visible={isOpen}
        transparent
        animationType="fade"
        onRequestClose={() => setIsOpen(false)}
      >
        <Pressable
          style={{
            flex: 1,
            backgroundColor: colors.overlay,
            justifyContent: "center",
            alignItems: "center",
            padding: 20,
          }}
          onPress={() => setIsOpen(false)}
        >
          <View
            style={{
              backgroundColor: colors.surface,
              borderRadius: 12,
              width: "100%",
              maxWidth: 320,
              maxHeight: "60%",
              overflow: "hidden",
            }}
          >
            <Text
              style={{
                fontSize: 18,
                fontWeight: "600",
                color: colors.text,
                padding: 16,
                borderBottomWidth: 1,
                borderBottomColor: colors.separator,
                textAlign: "center",
              }}
            >
              {label}
            </Text>
            <FlatList
              data={options}
              keyExtractor={(item) => String(item.value)}
              renderItem={({ item }) => (
                <Pressable
                  onPress={() => handleSelect(item.value)}
                  style={({ pressed }) => ({
                    paddingVertical: 14,
                    paddingHorizontal: 16,
                    backgroundColor:
                      item.value === value
                        ? colors.primaryTint
                        : pressed
                        ? colors.surfaceRecessed
                        : undefined,
                  })}
                >
                  <HStack justify="space-between" align="center">
                    <Text
                      style={{
                        fontSize: 16,
                        color: item.value === value ? colors.primaryOnTint : colors.text,
                        fontWeight: item.value === value ? "500" : "400",
                      }}
                    >
                      {item.name}
                    </Text>
                    {item.value === value && (
                      <Ionicons name="checkmark" size={20} color={colors.primaryOnTint} />
                    )}
                  </HStack>
                </Pressable>
              )}
              ItemSeparatorComponent={() => (
                <View
                  style={{
                    height: 1,
                    backgroundColor: colors.separator,
                    marginHorizontal: 16,
                  }}
                />
              )}
            />
          </View>
        </Pressable>
      </Modal>
    </VStack>
  );
};
