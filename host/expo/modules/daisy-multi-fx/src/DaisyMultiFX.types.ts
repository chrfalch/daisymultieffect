/**
 * Parameter definition for an effect
 */
export interface EffectParam {
  /** Parameter ID (0-7) */
  id: number;
  /** Human-readable name */
  name: string;

  /** Parameter kind (matches firmware ParamValueKind: 0=Number, 1=Enum, 2=File) */
  kind: number;

  /** Optional help/description text */
  description?: string;

  /** Optional unit prefix/suffix for display */
  unitPrefix?: string;
  unitSuffix?: string;

  /** Optional numeric range metadata (for kind==Number) */
  range?: {
    min: number;
    max: number;
    step: number;
  };
  
  /** Optional enum options (for kind==Enum) */
  enumOptions?: {
    value: number;
    name: string;
  }[];
}

/**
 * Effect metadata - describes what an effect type is and its parameters
 */
export interface EffectMeta {
  /** Effect type ID */
  typeId: number;
  /** Human-readable effect name */
  name: string;
  /** 3-character short name for display (e.g., "DLY", "REV", "OVD") */
  shortName: string;
  /** Optional effect description/help text */
  description?: string;
  /** Available parameters for this effect */
  params: EffectParam[];
}

/**
 * An effect slot in the patch
 */
export interface EffectSlot {
  /** Slot index (0-11) */
  slotIndex: number;
  /** Effect type ID (0 = off/empty) */
  typeId: number;
  /** Whether this slot is enabled */
  enabled: boolean;
  /** Left input routing */
  inputL: number;
  /** Right input routing */
  inputR: number;
  /** Sum inputs to mono */
  sumToMono: boolean;
  /** Dry level (0-127) */
  dry: number;
  /** Wet level (0-127) */
  wet: number;

  /** Channel routing policy (0=Auto, 1=ForceMono, 2=ForceStereo) */
  channelPolicy: number;
  /** Parameter values indexed by param ID */
  params: Record<number, number>;
}

/**
 * Complete patch state
 */
export interface Patch {
  /** Number of active slots */
  numSlots: number;
  /** All effect slots */
  slots: EffectSlot[];
  /** Input gain in dB (0 to +24 dB) */
  inputGainDb: number;
  /** Output gain in dB (-12 to +12 dB) */
  outputGainDb: number;
}

/**
 * MIDI connection status
 */
export interface ConnectionStatus {
  /** Whether MIDI is connected */
  connected: boolean;
  /** Number of MIDI sources found */
  sourceCount: number;
  /** Names of all MIDI sources */
  sourceNames: string[];
  /** Name of the current destination */
  destinationName: string;
  /** Status message */
  status: string;
}

/**
 * Device status with audio levels and CPU load
 */
export interface DeviceStatus {
  /** Input level (linear, 0.0-1.0+) */
  inputLevel: number;
  /** Output level (linear, 0.0-1.0+) */
  outputLevel: number;
  /** Average CPU load (0.0-1.0) */
  cpuAvg: number;
  /** Maximum CPU load since last reset (0.0-1.0) */
  cpuMax: number;
}

/**
 * Event payload for patch updates
 */
export interface PatchUpdateEvent {
  patch: Patch;
}

/**
 * Event payload for effect meta updates
 */
export interface EffectMetaUpdateEvent {
  effects: EffectMeta[];
}

/**
 * Event payload for connection status updates
 */
export interface ConnectionStatusEvent {
  status: ConnectionStatus;
}

/**
 * Event payload for device status updates (levels + CPU)
 */
export interface StatusUpdateEvent {
  status: DeviceStatus;
}
