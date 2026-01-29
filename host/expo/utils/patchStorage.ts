import { storage } from "./storage";
import type { Patch, EffectMeta } from "../modules/daisy-multi-fx";

const CURRENT_PATCH_KEY = "@daisymultifx/currentPatch";
const EFFECT_META_KEY = "@daisymultifx/effectMeta";

/** Schema version for effect metadata cache. Bump this when the EffectMeta structure changes. */
const EFFECT_META_SCHEMA_VERSION = 2;

/** Wrapper for versioned effect metadata storage */
interface VersionedEffectMeta {
  version: number;
  data: EffectMeta[];
}

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
    const versioned: VersionedEffectMeta = {
      version: EFFECT_META_SCHEMA_VERSION,
      data: effectMeta,
    };
    const json = JSON.stringify(versioned);
    storage.set(EFFECT_META_KEY, json);
  } catch (error) {
    console.warn("[patchStorage] Failed to save effect metadata:", error);
  }
}

/**
 * Load effect metadata from local storage.
 * This is a synchronous operation.
 * Returns empty array if the cached data is missing, invalid, or from an older schema version.
 *
 * @returns The saved effect metadata, or empty array if not saved, outdated, or on parse error
 */
export function loadEffectMeta(): EffectMeta[] {
  try {
    const json = storage.getString(EFFECT_META_KEY);
    if (!json) {
      return [];
    }
    const parsed = JSON.parse(json);

    // Check if it's versioned data
    if (
      typeof parsed === "object" &&
      parsed !== null &&
      "version" in parsed &&
      "data" in parsed
    ) {
      const versioned = parsed as VersionedEffectMeta;
      if (versioned.version === EFFECT_META_SCHEMA_VERSION) {
        return versioned.data;
      }
      // Outdated schema version - return empty to trigger fresh fetch
      console.log(
        `[patchStorage] Effect metadata schema outdated (v${versioned.version} vs v${EFFECT_META_SCHEMA_VERSION}), will refetch`
      );
      return [];
    }

    // Legacy unversioned data - return empty to trigger fresh fetch
    console.log(
      "[patchStorage] Effect metadata cache is unversioned, will refetch"
    );
    return [];
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
