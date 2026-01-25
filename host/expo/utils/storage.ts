import { MMKV } from "react-native-mmkv";

/**
 * Shared MMKV storage instance for the app.
 * MMKV provides fast, synchronous key-value storage.
 */
export const storage = new MMKV({
  id: "daisymultifx-storage",
});
