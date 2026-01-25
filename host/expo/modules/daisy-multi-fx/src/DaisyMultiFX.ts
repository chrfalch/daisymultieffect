import { EventSubscription } from "expo-modules-core";
import DaisyMultiFXModule from "./DaisyMultiFXModule";
import {
  Patch,
  EffectMeta,
  ConnectionStatus,
  DeviceStatus,
  PatchUpdateEvent,
  EffectMetaUpdateEvent,
  ConnectionStatusEvent,
  StatusUpdateEvent,
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
  DaisyMultiFXModule.requestEffectMeta();
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
  value: number,
): void {
  DaisyMultiFXModule.setSlotParam(slot, paramId, value);
}

/**
 * Set routing taps for a slot.
 * @param slot - Slot index (0-11)
 * @param inputL - Left input route (0-11 = slot, 255 = IN)
 * @param inputR - Right input route
 */
export function setSlotRouting(
  slot: number,
  inputL: number,
  inputR: number,
): void {
  DaisyMultiFXModule.setSlotRouting(slot, inputL, inputR);
}

/**
 * Set sum-to-mono for a slot.
 * @param slot - Slot index (0-11)
 * @param sumToMono - Whether to sum L+R to mono before processing
 */
export function setSlotSumToMono(slot: number, sumToMono: boolean): void {
  DaisyMultiFXModule.setSlotSumToMono(slot, sumToMono);
}

/**
 * Set dry/wet mix for a slot.
 * @param slot - Slot index (0-11)
 * @param dry - Dry level (0-127)
 * @param wet - Wet level (0-127)
 */
export function setSlotMix(slot: number, dry: number, wet: number): void {
  DaisyMultiFXModule.setSlotMix(slot, dry, wet);
}

/**
 * Set channel policy for a slot.
 * @param slot - Slot index (0-11)
 * @param channelPolicy - 0=Auto, 1=ForceMono, 2=ForceStereo
 */
export function setSlotChannelPolicy(
  slot: number,
  channelPolicy: number,
): void {
  DaisyMultiFXModule.setSlotChannelPolicy(slot, channelPolicy);
}

/**
 * Set input gain (pre-effects).
 * Used to boost instrument-level signals to line level.
 * @param gainDb - Gain in dB (0 to +24 dB)
 */
export function setInputGain(gainDb: number): void {
  DaisyMultiFXModule.setInputGain(gainDb);
}

/**
 * Set output gain (post-effects).
 * Used to adjust final output level.
 * @param gainDb - Gain in dB (-12 to +12 dB)
 */
export function setOutputGain(gainDb: number): void {
  DaisyMultiFXModule.setOutputGain(gainDb);
}

/**
 * Load a complete patch to the device/VST in a single MIDI message.
 * This is more efficient than sending individual commands for each slot.
 * @param patch - The complete patch to load
 */
export function loadPatch(patch: Patch): void {
  DaisyMultiFXModule.loadPatch(patch);
}

/**
 * Subscribe to patch updates.
 * Called when a new patch is received from the device.
 */
export function addPatchUpdateListener(
  listener: (event: PatchUpdateEvent) => void,
): EventSubscription {
  return DaisyMultiFXModule.addListener("onPatchUpdate", listener);
}

/**
 * Subscribe to effect metadata updates.
 * Called when effect metadata is received from the device.
 */
export function addEffectMetaUpdateListener(
  listener: (event: EffectMetaUpdateEvent) => void,
): EventSubscription {
  return DaisyMultiFXModule.addListener("onEffectMetaUpdate", listener);
}

/**
 * Subscribe to connection status changes.
 * Called when MIDI connection status changes.
 */
export function addConnectionStatusListener(
  listener: (event: ConnectionStatusEvent) => void,
): EventSubscription {
  return DaisyMultiFXModule.addListener("onConnectionStatusChange", listener);
}

/**
 * Subscribe to device status updates (audio levels + CPU load).
 * Called at ~30Hz when connected to the device.
 */
export function addStatusUpdateListener(
  listener: (event: StatusUpdateEvent) => void,
): EventSubscription {
  return DaisyMultiFXModule.addListener("onStatusUpdate", listener);
}
