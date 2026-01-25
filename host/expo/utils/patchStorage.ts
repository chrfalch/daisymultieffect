import { storage } from "./storage";
import type { Patch, EffectMeta } from "../modules/daisy-multi-fx";

const CURRENT_PATCH_KEY = "@daisymultifx/currentPatch";
const EFFECT_META_KEY = "@daisymultifx/effectMeta";

/**
 * Save the current patch to local storage.
 * This is a synchronous operation.
 *
 * @param patch - The patch to save
 */
export function saveCurrentPatch(patch: Patch): void {
  try {
    const json = JSON.stringify(patch);
    storage.set(CURRENT_PATCH_KEY, json);
  } catch (error) {
    console.warn("[patchStorage] Failed to save patch:", error);
  }
}

/**
 * Load the current patch from local storage.
 * This is a synchronous operation.
 *
 * @returns The saved patch, or null if no patch is saved or on parse error
 */
export function loadCurrentPatch(): Patch | null {
  try {
    const json = storage.getString(CURRENT_PATCH_KEY);
    if (!json) {
      return null;
    }
    return JSON.parse(json) as Patch;
  } catch (error) {
    console.warn("[patchStorage] Failed to load patch:", error);
    return null;
  }
}

/**
 * Clear the saved patch from local storage.
 */
export function clearCurrentPatch(): void {
  storage.delete(CURRENT_PATCH_KEY);
}

/**
 * Save effect metadata to local storage.
 * This is a synchronous operation.
 *
 * @param effectMeta - The effect metadata array to save
 */
export function saveEffectMeta(effectMeta: EffectMeta[]): void {
  try {
    const json = JSON.stringify(effectMeta);
    storage.set(EFFECT_META_KEY, json);
  } catch (error) {
    console.warn("[patchStorage] Failed to save effect metadata:", error);
  }
}

/**
 * Load effect metadata from local storage.
 * This is a synchronous operation.
 *
 * @returns The saved effect metadata, or empty array if not saved or on parse error
 */
export function loadEffectMeta(): EffectMeta[] {
  try {
    const json = storage.getString(EFFECT_META_KEY);
    if (!json) {
      return [];
    }
    return JSON.parse(json) as EffectMeta[];
  } catch (error) {
    console.warn("[patchStorage] Failed to load effect metadata:", error);
    return [];
  }
}

/**
 * Clear the saved effect metadata from local storage.
 */
export function clearEffectMeta(): void {
  storage.delete(EFFECT_META_KEY);
}
