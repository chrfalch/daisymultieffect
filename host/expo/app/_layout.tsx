import { NativeTabs } from "expo-router/unstable-native-tabs";
import { ThemeProvider } from "../components/ThemeProvider";
import { GestureHandlerRootView } from "react-native-gesture-handler";

export default function RootLayout() {
  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <ThemeProvider>
        <NativeTabs minimizeBehavior="onScrollDown">
          <NativeTabs.Trigger name="(pedalboard)">
            <NativeTabs.Trigger.Icon sf="slider.horizontal.3" />
            <NativeTabs.Trigger.Label>Pedalboard</NativeTabs.Trigger.Label>
          </NativeTabs.Trigger>
          <NativeTabs.Trigger name="(settings)">
            <NativeTabs.Trigger.Icon sf="gear" />
            <NativeTabs.Trigger.Label>Settings</NativeTabs.Trigger.Label>
          </NativeTabs.Trigger>
        </NativeTabs>
      </ThemeProvider>
    </GestureHandlerRootView>
  );
}
