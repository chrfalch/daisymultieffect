import DaisyMultiFXModule from "./src/DaisyMultiFXModule";

export type {
  EffectSlot,
  EffectMeta,
  EffectParam,
  Patch,
  ConnectionStatus,
  DeviceStatus,
} from "./src/DaisyMultiFX.types";

export {
  // Initialization
  initialize,
  disconnect,
  // Connection
  getConnectionStatus,
  isConnected,
  // Patch operations
  requestPatch,
  requestEffectMeta,
  getPatch,
  getEffectMeta,
  // Slot control
  setSlotEnabled,
  setSlotType,
  setSlotParam,
  setSlotRouting,
  setSlotSumToMono,
  setSlotMix,
  setSlotChannelPolicy,
  // Events
  addPatchUpdateListener,
  addEffectMetaUpdateListener,
  addConnectionStatusListener,
  addStatusUpdateListener,
} from "./src/DaisyMultiFX";

export default DaisyMultiFXModule;
