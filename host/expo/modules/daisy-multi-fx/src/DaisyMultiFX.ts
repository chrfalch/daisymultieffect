import { Subscription } from "expo-modules-core";
import DaisyMultiFXModule from "./DaisyMultiFXModule";
import {
  Patch,
  EffectMeta,
  ConnectionStatus,
  PatchUpdateEvent,
  EffectMetaUpdateEvent,
  ConnectionStatusEvent,
} from "./DaisyMultiFX.types";

/**
 * Initialize the MIDI connection.
 * Call this when your app starts to set up CoreMIDI.
 */
export async function initialize(): Promise<void> {
  return DaisyMultiFXModule.initialize();
}

/**
 * Disconnect from MIDI.
 * Call this when your app is closing.
 */
export async function disconnect(): Promise<void> {
  return DaisyMultiFXModule.disconnect();
}

/**
 * Get the current MIDI connection status.
 */
export function getConnectionStatus(): ConnectionStatus {
  return DaisyMultiFXModule.getConnectionStatus();
}

/**
 * Check if MIDI is currently connected.
 */
export function isConnected(): boolean {
  return DaisyMultiFXModule.isConnected();
}

/**
 * Request the current patch from the device/VST.
 * Response will be delivered via onPatchUpdate event.
 */
export function requestPatch(): void {
  DaisyMultiFXModule.requestPatch();
}

/**
 * Request effect metadata from the device/VST.
 * Response will be delivered via onEffectMetaUpdate event.
 */
export function requestEffectMeta(): void {
  DaisyMultiFXModule.requestEffectMeta();
}

/**
 * Get the current patch state (synchronous).
 * Returns null if no patch has been received yet.
 */
export function getPatch(): Patch | null {
  return DaisyMultiFXModule.getPatch();
}

/**
 * Get all available effect metadata (synchronous).
 */
export function getEffectMeta(): EffectMeta[] {
  return DaisyMultiFXModule.getEffectMeta();
}

/**
 * Enable or disable an effect slot.
 * @param slot - Slot index (0-11)
 * @param enabled - Whether the slot should be enabled
 */
export function setSlotEnabled(slot: number, enabled: boolean): void {
  DaisyMultiFXModule.setSlotEnabled(slot, enabled);
}

/**
 * Set the effect type for a slot.
 * @param slot - Slot index (0-11)
 * @param typeId - Effect type ID (0 = off)
 */
export function setSlotType(slot: number, typeId: number): void {
  DaisyMultiFXModule.setSlotType(slot, typeId);
}

/**
 * Set a parameter value for a slot.
 * @param slot - Slot index (0-11)
 * @param paramId - Parameter ID (0-7)
 * @param value - Parameter value (0-127)
 */
export function setSlotParam(
  slot: number,
  paramId: number,
  value: number
): void {
  DaisyMultiFXModule.setSlotParam(slot, paramId, value);
}

/**
 * Subscribe to patch updates.
 * Called when a new patch is received from the device.
 */
export function addPatchUpdateListener(
  listener: (event: PatchUpdateEvent) => void
): Subscription {
  return DaisyMultiFXModule.addListener("onPatchUpdate", listener);
}

/**
 * Subscribe to effect metadata updates.
 * Called when effect metadata is received from the device.
 */
export function addEffectMetaUpdateListener(
  listener: (event: EffectMetaUpdateEvent) => void
): Subscription {
  return DaisyMultiFXModule.addListener("onEffectMetaUpdate", listener);
}

/**
 * Subscribe to connection status changes.
 * Called when MIDI connection status changes.
 */
export function addConnectionStatusListener(
  listener: (event: ConnectionStatusEvent) => void
): Subscription {
  return DaisyMultiFXModule.addListener("onConnectionStatusChange", listener);
}
