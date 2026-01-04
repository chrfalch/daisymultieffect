import React from "react";
import { View, StyleProp, ViewStyle, ViewProps } from "react-native";

export type StackDirection = "horizontal" | "vertical";

export interface StackProps extends Omit<ViewProps, "style" | "children"> {
  children?: React.ReactNode;

  direction?: StackDirection;

  align?: ViewStyle["alignItems"];
  justify?: ViewStyle["justifyContent"];

  gap?: number;

  padding?: number;
  paddingHorizontal?: number;
  paddingVertical?: number;

  style?: StyleProp<ViewStyle>;
}

export const Stack: React.FC<StackProps> = ({
  children,
  direction = "vertical",
  align,
  justify,
  gap,
  padding,
  paddingHorizontal,
  paddingVertical,
  style,
  ...viewProps
}) => {
  const stackStyle: ViewStyle = {
    flexDirection: direction === "horizontal" ? "row" : "column",
    ...(align != null ? { alignItems: align } : null),
    ...(justify != null ? { justifyContent: justify } : null),
    ...(gap != null ? { gap } : null),
    ...(padding != null ? { padding } : null),
    ...(paddingHorizontal != null ? { paddingHorizontal } : null),
    ...(paddingVertical != null ? { paddingVertical } : null),
  };

  return (
    <View {...viewProps} style={[stackStyle, style]}>
      {children}
    </View>
  );
};

export type HStackProps = Omit<StackProps, "direction">;
export const HStack: React.FC<HStackProps> = (props) => {
  return <Stack {...props} direction="horizontal" />;
};

export type VStackProps = Omit<StackProps, "direction">;
export const VStack: React.FC<VStackProps> = (props) => {
  return <Stack {...props} direction="vertical" />;
};

export interface WrapStackProps extends Omit<StackProps, "direction"> {
  direction?: StackDirection;
}

export const WrapStack: React.FC<WrapStackProps> = ({
  direction = "horizontal",
  style,
  ...rest
}) => {
  return (
    <Stack
      {...rest}
      direction={direction}
      style={[{ flexWrap: "wrap" }, style]}
    />
  );
};
