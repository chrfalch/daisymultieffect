import { NativeModule, requireNativeModule } from "expo-modules-core";

import {
  Patch,
  EffectMeta,
  ConnectionStatus,
  PatchUpdateEvent,
  EffectMetaUpdateEvent,
  ConnectionStatusEvent,
} from "./DaisyMultiFX.types";

declare class DaisyMultiFXModuleType extends NativeModule<{
  onPatchUpdate: (event: PatchUpdateEvent) => void;
  onEffectMetaUpdate: (event: EffectMetaUpdateEvent) => void;
  onConnectionStatusChange: (event: ConnectionStatusEvent) => void;
}> {
  // Initialization
  initialize(): Promise<void>;
  disconnect(): Promise<void>;

  // Connection status
  getConnectionStatus(): ConnectionStatus;
  isConnected(): boolean;

  // Requests
  requestPatch(): void;
  requestEffectMeta(): void;

  // State getters
  getPatch(): Patch | null;
  getEffectMeta(): EffectMeta[];

  // Slot control
  setSlotEnabled(slot: number, enabled: boolean): void;
  setSlotType(slot: number, typeId: number): void;
  setSlotParam(slot: number, paramId: number, value: number): void;
}

export default requireNativeModule<DaisyMultiFXModuleType>("DaisyMultiFX");
