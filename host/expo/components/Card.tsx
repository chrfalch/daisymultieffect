import React from "react";
import { View, StyleSheet, ViewStyle, StyleProp } from "react-native";

interface CardProps {
  children: React.ReactNode;
  selected?: boolean;
  style?: StyleProp<ViewStyle>;
}

export const Card: React.FC<CardProps> = ({
  children,
  selected = false,
  style,
}) => {
  return (
    <View style={[styles.card, selected && styles.cardSelected, style]}>
      {children}
    </View>
  );
};

const styles = StyleSheet.create({
  card: {
    backgroundColor: "#fff",
    padding: 16,
    borderRadius: 12,
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
    borderWidth: 2,
    borderColor: "#EEEEEE",
  },
  cardSelected: {
    backgroundColor: "#E3F2FD",
    borderWidth: 2,
    borderColor: "#2196F3",
  },
});
