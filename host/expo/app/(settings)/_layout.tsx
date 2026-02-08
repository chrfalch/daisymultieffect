import { Stack } from "expo-router/stack";
import { useThemeColors } from "../../components/ThemeProvider";

export default function SettingsLayout() {
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
          title: "Settings",
          headerLargeTitle: true,
        }}
      />
    </Stack>
  );
}
