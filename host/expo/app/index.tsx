import { StyleSheet, Text, View } from "react-native";
import { EditorScreen } from "../screens/editor";

export default function Page() {
  return (
    <View style={styles.container}>
      <EditorScreen />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
});
