/**
 * SysExProtocol.swift
 * Swift SysEx protocol definitions for DaisyMultiFX
 *
 * IMPORTANT: These values must match core/protocol/sysex_protocol_c.h
 * If you change protocol values, update both files!
 *
 * All messages use manufacturer ID 0x7D (educational/development use).
 */

import Foundation

// MARK: - Constants
// These values must match core/protocol/sysex_protocol_c.h

/// MIDI SysEx manufacturer ID (0x7D = educational/development use)
let kSysExManufacturerID: UInt8 = 0x7D

/// Maximum number of effect slots
let kMaxSlots: UInt8 = 12

/// Maximum parameters per effect slot
let kMaxParamsPerSlot: UInt8 = 8

/// Number of hardware buttons
let kNumButtons: UInt8 = 2

/// Special route value indicating hardware input
let kRouteInput: UInt8 = 255

// MARK: - Message Types - Host to Device (Commands)

enum SysExCommand {
    /// Request patch dump: F0 7D 12 F7
    static let requestPatch: UInt8 = 0x12

    /// Set parameter: F0 7D 20 <slot> <paramId> <value> F7
    static let setParam: UInt8 = 0x20

    /// Set slot enabled: F0 7D 21 <slot> <enabled> F7
    static let setEnabled: UInt8 = 0x21

    /// Set slot effect type: F0 7D 22 <slot> <typeId> F7
    static let setType: UInt8 = 0x22

    /// Request all effect metadata: F0 7D 32 F7
    static let requestEffectMeta: UInt8 = 0x32
}

// MARK: - Message Types - Device to Host (Responses)

enum SysExResponse {
    /// Patch dump response
    static let patchDump: UInt8 = 0x13

    /// Effect discovered (single effect)
    static let effectDiscovered: UInt8 = 0x34

    /// Effect metadata v2 (with params)
    static let effectMetaV2: UInt8 = 0x35

    /// Effect metadata v3 (with params and short name)
    static let effectMetaV3: UInt8 = 0x36

    /// Effect meta list (all effects)
    static let effectMetaList: UInt8 = 0x33

    /// Button state change
    static let buttonState: UInt8 = 0x40

    /// Tempo update
    static let tempoUpdate: UInt8 = 0x41
}

// MARK: - Effect Type IDs

enum EffectTypeID {
    static let off: UInt8 = 0
    static let delay: UInt8 = 1
    static let distortion: UInt8 = 10
    static let sweepDelay: UInt8 = 12
    static let mixer: UInt8 = 13
    static let reverb: UInt8 = 14
    static let compressor: UInt8 = 15
    static let chorus: UInt8 = 16
}

// MARK: - Enums

/// Channel routing policy for stereo effects
enum ChannelPolicy: UInt8 {
    case auto = 0
    /// Automatic based on input
    case forceMono = 1
    /// Force mono processing
    case forceStereo = 2/// Force stereo processing
}

/// Hardware button mode assignment
enum ButtonMode: UInt8 {
    case unused = 0
    /// Button not assigned
    case toggleBypass = 1
    /// Toggle effect bypass
    case tapTempo = 2/// Tap tempo input
}

// MARK: - Wire Structures

/// Button assignment for hardware buttons
struct ButtonAssignWire: Hashable {
    var slotIndex: UInt8
    /// 0-11 or 127 for unassigned
    var mode: ButtonMode
}

/// Single parameter (id + value pair)
struct SlotParamWire: Hashable {
    var id: UInt8
    /// Parameter ID within effect
    var value: UInt8/// 0..127
}

/// Complete slot description
struct SlotWireDesc: Hashable {
    var slotIndex: UInt8
    /// 0-11
    var typeId: UInt8
    /// Effect type (see EffectTypeID)
    var enabled: UInt8
    /// 0/1

    var inputL: UInt8
    /// Left input route (0-11 = slot, kRouteInput = hw input)
    var inputR: UInt8
    /// Right input route

    var sumToMono: UInt8
    /// Sum inputs to mono (0/1)
    var dry: UInt8
    /// Dry level 0..127
    var wet: UInt8
    /// Wet level 0..127

    var channelPolicy: UInt8
    /// ChannelPolicy raw value
    var numParams: UInt8
    /// Number of valid params
    var params: [SlotParamWire]

    init() {
        slotIndex = 0
        typeId = 0
        enabled = 0
        inputL = kRouteInput
        inputR = kRouteInput
        sumToMono = 0
        dry = 0
        wet = 127
        channelPolicy = 0
        numParams = 0
        params = []
    }
}

/// Complete patch description
struct PatchWireDesc: Hashable {
    var numSlots: UInt8
    var slots: [SlotWireDesc]
    var buttons: [ButtonAssignWire]

    init() {
        numSlots = 4
        slots = (0..<12).map { i in
            var slot = SlotWireDesc()
            slot.slotIndex = UInt8(i)
            return slot
        }
        buttons = [
            ButtonAssignWire(slotIndex: 127, mode: .unused),
            ButtonAssignWire(slotIndex: 127, mode: .unused),
        ]
    }
}

// MARK: - Utility Functions

/// Encode a route value for 7-bit safe transmission
func encodeRoute(_ route: UInt8) -> UInt8 {
    return (route == kRouteInput) ? 127 : (route & 0x7F)
}

/// Decode a route value from 7-bit safe transmission
func decodeRoute(_ encoded: UInt8) -> UInt8 {
    return (encoded == 127) ? kRouteInput : encoded
}

/// Convert float to Q16.16 fixed point
func floatToQ16_16(_ v: Float) -> Int32 {
    return Int32(v * 65536.0 + (v >= 0 ? 0.5 : -0.5))
}

/// Convert Q16.16 fixed point to float
func q16_16ToFloat(_ v: Int32) -> Float {
    return Float(v) / 65536.0
}

/// Pack Q16.16 value into 5 bytes (7-bit safe)
func packQ16_16(_ value: Int32) -> [UInt8] {
    let u = UInt32(bitPattern: value)
    return [
        UInt8(u & 0x7F),
        UInt8((u >> 7) & 0x7F),
        UInt8((u >> 14) & 0x7F),
        UInt8((u >> 21) & 0x7F),
        UInt8((u >> 28) & 0x7F),
    ]
}

/// Unpack 5 bytes into Q16.16 value
func unpackQ16_16(_ bytes: [UInt8]) -> Int32 {
    guard bytes.count >= 5 else { return 0 }
    var u: UInt32 = 0
    u |= UInt32(bytes[0])
    u |= UInt32(bytes[1]) << 7
    u |= UInt32(bytes[2]) << 14
    u |= UInt32(bytes[3]) << 21
    u |= UInt32(bytes[4]) << 28
    return Int32(bitPattern: u)
}
