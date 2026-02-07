import { Stack } from "expo-router/stack";
import { useThemeColors } from "../../components/ThemeProvider";

export default function PedalboardLayout() {
  const colors = useThemeColors();

  return (
    <Stack
      screenOptions={{
        headerTransparent: true,
        headerShadowVisible: false,
        headerLargeTitleShadowVisible: false,
        headerLargeStyle: { backgroundColor: "transparent" },
        headerBlurEffect: "none",
        headerBackButtonDisplayMode: "minimal",
        contentStyle: { backgroundColor: colors.background },
      }}
    >
      <Stack.Screen
        name="index"
        options={{
          title: "Pedalboard",
          headerLargeTitle: true,
        }}
      />
      <Stack.Screen
        name="slot-params"
        options={{
          presentation: "formSheet",
          sheetGrabberVisible: true,
          sheetAllowedDetents: [0.5, 1.0],
          headerTransparent: true,
          contentStyle: { backgroundColor: "transparent" },
          title: "",
        }}
      />
      <Stack.Screen
        name="slot-routing"
        options={{
          presentation: "formSheet",
          sheetGrabberVisible: true,
          sheetAllowedDetents: [0.5, 1.0],
          headerTransparent: true,
          contentStyle: { backgroundColor: "transparent" },
          title: "",
        }}
      />
    </Stack>
  );
}
