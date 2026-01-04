import { useEffect, useState, useCallback } from "react";
import {
  initialize,
  disconnect,
  getConnectionStatus,
  getPatch,
  getEffectMeta,
  requestPatch,
  requestEffectMeta,
  setSlotEnabled,
  setSlotType,
  setSlotParam,
  addPatchUpdateListener,
  addEffectMetaUpdateListener,
  addConnectionStatusListener,
} from "../modules/daisy-multi-fx";
import type {
  Patch,
  EffectMeta,
  ConnectionStatus,
  EffectSlot,
  EffectParam,
} from "../modules/daisy-multi-fx";

export interface UseDaisyMultiFXResult {
  // Connection
  isConnected: boolean;
  connectionStatus: ConnectionStatus | null;

  // State
  patch: Patch | null;
  effectMeta: EffectMeta[];

  // Actions
  initialize: () => Promise<void>;
  disconnect: () => Promise<void>;
  refreshPatch: () => void;
  refreshEffectMeta: () => void;

  // Slot control
  setSlotEnabled: (slot: number, enabled: boolean) => void;
  setSlotType: (slot: number, typeId: number) => void;
  setSlotParam: (slot: number, paramId: number, value: number) => void;

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
      requestPatch();
      requestEffectMeta();
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

    // Auto-initialize if requested
    if (autoInitialize) {
      initializeMidi();
    }

    // Cleanup
    return () => {
      patchSub.remove();
      metaSub.remove();
      statusSub.remove();
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
      return param?.name;
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

  return {
    // Connection
    isConnected,
    connectionStatus,

    // State
    patch,
    effectMeta,

    // Actions
    initialize: initializeMidi,
    disconnect: disconnectMidi,
    refreshPatch: requestPatch,
    refreshEffectMeta: requestEffectMeta,

    // Slot control
    setSlotEnabled,
    setSlotType,
    setSlotParam,

    // Helpers
    getSlot,
    getEffectName,
    getParamName,
    getParamMeta,
    getEffectShortName,
  };
}
