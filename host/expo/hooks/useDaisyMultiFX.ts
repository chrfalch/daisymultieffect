import { useEffect, useState, useCallback } from "react";
import {
  initialize,
  disconnect,
  getConnectionStatus,
  getPatch,
  getEffectMeta,
  requestPatch as refreshPatch,
  requestEffectMeta as refreshEffectMeta,
  setSlotEnabled,
  setSlotType as setSlotTypeRaw,
  setSlotParam,
  setSlotRouting as setSlotRoutingRaw,
  setSlotSumToMono,
  setSlotMix,
  setSlotChannelPolicy,
  addPatchUpdateListener,
  addEffectMetaUpdateListener,
  addConnectionStatusListener,
  addStatusUpdateListener,
  requestEffectMeta,
} from "../modules/daisy-multi-fx";
import type {
  Patch,
  EffectMeta,
  ConnectionStatus,
  DeviceStatus,
  EffectSlot,
  EffectParam,
} from "../modules/daisy-multi-fx";

export interface UseDaisyMultiFXResult {
  // Connection
  isConnected: boolean;
  connectionStatus: ConnectionStatus | null;

  // Device status (levels + CPU)
  deviceStatus: DeviceStatus | null;

  // State
  patch: Patch | null;
  effectMeta: EffectMeta[];

  // Actions
  initialize: () => Promise<void>;
  disconnect: () => Promise<void>;
  refreshPatch: () => void;
  refreshEffectMeta: () => void;
  pushPatchToVst: () => void;

  // Slot control
  setSlotEnabled: (slot: number, enabled: boolean) => void;
  setSlotType: (slot: number, typeId: number) => void;
  setSlotParam: (slot: number, paramId: number, value: number) => void;

  // Routing/mix control
  setSlotRouting: (slot: number, inputL: number, inputR: number) => void;
  setSlotSumToMono: (slot: number, sumToMono: boolean) => void;
  setSlotMix: (slot: number, dry: number, wet: number) => void;
  setSlotChannelPolicy: (slot: number, channelPolicy: number) => void;

  // Helpers
  getSlot: (slotIndex: number) => EffectSlot | undefined;
  getEffectName: (typeId: number) => string;
  getParamName: (typeId: number, paramId: number) => string;
  getParamMeta: (typeId: number, paramId: number) => EffectParam | undefined;
  getEffectShortName: (typeId: number) => string;
}

/**
 * React hook for using the DaisyMultiFX module.
 *
 * Handles initialization, event subscriptions, and provides a clean API.
 *
 * @param autoInitialize - If true, automatically initializes MIDI on mount (default: true)
 */
export function useDaisyMultiFX(
  autoInitialize: boolean = true
): UseDaisyMultiFXResult {
  const [isConnected, setIsConnected] = useState(false);
  const [connectionStatus, setConnectionStatus] =
    useState<ConnectionStatus | null>(null);
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatus | null>(null);
  const [patch, setPatch] = useState<Patch | null>(null);
  const [effectMeta, setEffectMeta] = useState<EffectMeta[]>([]);

  // Initialize MIDI
  const initializeMidi = useCallback(async () => {
    try {
      await initialize();

      // Get initial state
      const status = getConnectionStatus();
      setConnectionStatus(status);
      setIsConnected(status.connected);

      const currentPatch = getPatch();
      if (currentPatch) {
        setPatch(currentPatch);
      }

      const currentMeta = getEffectMeta();
      if (currentMeta.length > 0) {
        setEffectMeta(currentMeta);
      }

      // Request fresh data
      refreshPatch();
    } catch (error) {
      console.error("[useDaisyMultiFX] Initialize failed:", error);
    }
  }, []);

  // Disconnect MIDI
  const disconnectMidi = useCallback(async () => {
    try {
      await disconnect();
      setIsConnected(false);
      setConnectionStatus(null);
    } catch (error) {
      console.error("[useDaisyMultiFX] Disconnect failed:", error);
    }
  }, []);

  // Set up event listeners
  useEffect(() => {
    const patchSub = addPatchUpdateListener((event) => {
      setPatch(event.patch);
    });

    const metaSub = addEffectMetaUpdateListener((event) => {
      setEffectMeta(event.effects);
    });

    const statusSub = addConnectionStatusListener((event) => {
      setConnectionStatus(event.status);
      setIsConnected(event.status.connected);
    });

    const deviceStatusSub = addStatusUpdateListener((event) => {
      setDeviceStatus(event.status);
    });

    // Auto-initialize if requested
    if (autoInitialize) {
      initializeMidi();
    }

    // Cleanup
    return () => {
      patchSub.remove();
      metaSub.remove();
      statusSub.remove();
      deviceStatusSub.remove();
    };
  }, [autoInitialize, initializeMidi]);

  // Helper: Get a slot by index
  const getSlot = useCallback(
    (slotIndex: number): EffectSlot | undefined => {
      return patch?.slots.find((s) => s.slotIndex === slotIndex);
    },
    [patch]
  );

  // Helper: Get effect name by type ID
  const getEffectName = useCallback(
    (typeId: number): string => {
      if (typeId === 0) return "Off";
      const effect = effectMeta.find((e) => e.typeId === typeId);
      return effect?.name ?? `Effect ${typeId}`;
    },
    [effectMeta]
  );

  // Helper: Get effect name by type ID
  const getEffectShortName = useCallback(
    (typeId: number): string => {
      if (typeId === 0) return "Off";
      const effect = effectMeta.find((e) => e.typeId === typeId);
      return effect?.shortName ?? `Effect ${typeId}`;
    },
    [effectMeta]
  );

  // Helper: Get parameter name
  const getParamName = useCallback(
    (typeId: number, paramId: number): string => {
      const effect = effectMeta.find((e) => e.typeId === typeId);
      const param = effect?.params.find((p) => p.id === paramId);
      const name = (param?.name ?? "").trim();
      return name.length > 0 ? name : null;
    },
    [effectMeta]
  );

  const getParamMeta = useCallback(
    (typeId: number, paramId: number) => {
      const effect = effectMeta.find((e) => e.typeId === typeId);
      return effect?.params.find((p) => p.id === paramId);
    },
    [effectMeta]
  );

  const setSlotType = useCallback(
    (slot: number, typeId: number) => {
      const currentSlot = patch?.slots.find((s) => s.slotIndex === slot);
      const inputL = currentSlot?.inputL;
      const inputR = currentSlot?.inputR;

      setSlotTypeRaw(slot, typeId);

      // Preserve routing across type changes by re-applying the previous routing.
      if (typeof inputL === "number" && typeof inputR === "number") {
        setSlotRoutingRaw(slot, inputL, inputR);
      }
    },
    [patch]
  );

  const pushPatchToVst = useCallback(() => {
    if (!patch) {
      console.warn("[useDaisyMultiFX] pushPatchToVst: no patch loaded");
      return;
    }
    if (!isConnected) {
      console.warn("[useDaisyMultiFX] pushPatchToVst: not connected");
      return;
    }

    requestEffectMeta();

    // Re-send the full current patch state using existing per-slot commands.
    // The MIDI protocol doesn't currently support a single 'LOAD_PATCH' message.
    for (const slot of patch.slots) {
      const slotIndex = slot.slotIndex;

      // Type first (may reset/init defaults on the receiver)
      setSlotTypeRaw(slotIndex, slot.typeId);
      setSlotEnabled(slotIndex, slot.enabled);

      // Routing + mix/state
      setSlotRoutingRaw(slotIndex, slot.inputL, slot.inputR);
      setSlotSumToMono(slotIndex, slot.sumToMono);
      setSlotMix(slotIndex, slot.dry, slot.wet);
      setSlotChannelPolicy(slotIndex, slot.channelPolicy);

      // Parameters
      const entries = Object.entries(slot.params)
        .map(([paramId, value]) => [Number(paramId), value] as const)
        .filter(([paramId]) => Number.isFinite(paramId))
        .sort((a, b) => a[0] - b[0]);
      for (const [paramId, value] of entries) {
        setSlotParam(slotIndex, paramId, value);
      }
    }
  }, [patch, isConnected]);

  return {
    // Connection
    isConnected,
    connectionStatus,

    // Device status (levels + CPU)
    deviceStatus,

    // State
    patch,
    effectMeta,

    // Actions
    initialize: initializeMidi,
    disconnect: disconnectMidi,
    refreshPatch,
    refreshEffectMeta,
    pushPatchToVst,

    // Slot control
    setSlotEnabled,
    setSlotType,
    setSlotParam,

    // Routing/mix control
    setSlotRouting: setSlotRoutingRaw,
    setSlotSumToMono,
    setSlotMix,
    setSlotChannelPolicy,

    // Helpers
    getSlot,
    getEffectName,
    getParamName,
    getParamMeta,
    getEffectShortName,
  };
}
