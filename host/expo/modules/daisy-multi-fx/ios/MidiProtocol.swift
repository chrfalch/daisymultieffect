import Foundation

/// MIDI Protocol definitions matching C++ MidiProtocol
enum MidiProtocol {
    static let manufacturerId: UInt8 = 0x7D

    // SysEx data bytes must be 7-bit. The firmware encodes ROUTE_INPUT (255) as 127 on-wire.
    static func encodeRoute(_ route: UInt8) -> UInt8 {
        (route == 255) ? 127 : (route & 0x7F)
    }

    static func decodeRoute(_ encoded: UInt8) -> UInt8 {
        (encoded == 127) ? 255 : encoded
    }

    enum Sender {
        static let firmware: UInt8 = 0x01
        static let vst: UInt8 = 0x02
        static let swift: UInt8 = 0x03
    }

    enum Cmd {
        static let requestPatch: UInt8 = 0x12
        static let loadPatch: UInt8 = 0x14
        static let setParam: UInt8 = 0x20
        static let setEnabled: UInt8 = 0x21
        static let setType: UInt8 = 0x22
        static let setRouting: UInt8 = 0x23
        static let setSumToMono: UInt8 = 0x24
        static let setMix: UInt8 = 0x25
        static let setChannelPolicy: UInt8 = 0x26
        static let setInputGain: UInt8 = 0x27
        static let setOutputGain: UInt8 = 0x28
        static let setGlobalBypass: UInt8 = 0x29
        static let requestMeta: UInt8 = 0x32
    }

    enum Resp {
        static let patchDump: UInt8 = 0x13
        static let effectMeta: UInt8 = 0x33
        static let effectDiscovered: UInt8 = 0x34
        static let effectMetaV2: UInt8 = 0x35
        static let effectMetaV3: UInt8 = 0x36
        static let effectMetaV4: UInt8 = 0x37
        static let effectMetaV5: UInt8 = 0x38
        static let buttonState: UInt8 = 0x40
        static let tempoUpdate: UInt8 = 0x41
        static let statusUpdate: UInt8 = 0x42
    }

    // MARK: - Encoders

    static func encodeSetEnabled(slot: UInt8, enabled: Bool) -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.setEnabled, slot & 0x7F, enabled ? 1 : 0, 0xF7]
    }

    static func encodeSetType(slot: UInt8, typeId: UInt8) -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.setType, slot & 0x7F, typeId & 0x7F, 0xF7]
    }

    static func encodeSetParam(slot: UInt8, paramId: UInt8, value: UInt8) -> [UInt8] {
        [
            0xF0, manufacturerId, Sender.swift, Cmd.setParam, slot & 0x7F, paramId & 0x7F,
            value & 0x7F, 0xF7,
        ]
    }

    static func encodeRequestPatch() -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.requestPatch, 0xF7]
    }

    static func encodeRequestMeta() -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.requestMeta, 0xF7]
    }

    static func encodeSetRouting(slot: UInt8, inputL: UInt8, inputR: UInt8) -> [UInt8] {
        [
            0xF0, manufacturerId, Sender.swift, Cmd.setRouting, slot & 0x7F,
            encodeRoute(inputL),
            encodeRoute(inputR),
            0xF7,
        ]
    }

    static func encodeSetSumToMono(slot: UInt8, sumToMono: Bool) -> [UInt8] {
        [
            0xF0, manufacturerId, Sender.swift, Cmd.setSumToMono, slot & 0x7F, sumToMono ? 1 : 0,
            0xF7,
        ]
    }

    static func encodeSetMix(slot: UInt8, dry: UInt8, wet: UInt8) -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.setMix, slot & 0x7F, dry & 0x7F, wet & 0x7F, 0xF7]
    }

    static func encodeSetChannelPolicy(slot: UInt8, channelPolicy: UInt8) -> [UInt8] {
        [
            0xF0, manufacturerId, Sender.swift, Cmd.setChannelPolicy, slot & 0x7F,
            channelPolicy & 0x7F, 0xF7,
        ]
    }

    /// Encode SET_INPUT_GAIN command (gain in dB, range 0-24)
    static func encodeSetInputGain(gainDb: Float) -> [UInt8] {
        var msg: [UInt8] = [0xF0, manufacturerId, Sender.swift, Cmd.setInputGain]
        msg.append(contentsOf: packQ16_16(floatToQ16_16(gainDb)))
        msg.append(0xF7)
        return msg
    }

    /// Encode SET_OUTPUT_GAIN command (gain in dB, range -12 to +12)
    static func encodeSetOutputGain(gainDb: Float) -> [UInt8] {
        var msg: [UInt8] = [0xF0, manufacturerId, Sender.swift, Cmd.setOutputGain]
        msg.append(contentsOf: packQ16_16(floatToQ16_16(gainDb)))
        msg.append(0xF7)
        return msg
    }

    /// Encode SET_GLOBAL_BYPASS command
    static func encodeSetGlobalBypass(bypass: Bool) -> [UInt8] {
        [0xF0, manufacturerId, Sender.swift, Cmd.setGlobalBypass, bypass ? 1 : 0, 0xF7]
    }

    /// Encode LOAD_PATCH command - sends complete patch in single message (~332 bytes)
    /// Same wire format as PATCH_DUMP but with command code 0x14
    static func encodeLoadPatch(patch: Patch) -> [UInt8] {
        var msg: [UInt8] = [0xF0, manufacturerId, Sender.swift, Cmd.loadPatch]

        // numSlots
        msg.append(UInt8(patch.numSlots) & 0x7F)

        // Always encode 12 slots
        for i in 0..<12 {
            if i < patch.slots.count {
                let slot = patch.slots[i]
                msg.append(UInt8(i) & 0x7F)  // slotIndex
                msg.append(slot.typeId & 0x7F)
                msg.append(slot.enabled ? 1 : 0)
                msg.append(encodeRoute(slot.inputL))
                msg.append(encodeRoute(slot.inputR))
                msg.append(slot.sumToMono ? 1 : 0)
                msg.append(slot.dry & 0x7F)
                msg.append(slot.wet & 0x7F)
                msg.append(slot.channelPolicy & 0x7F)

                // Send all params that are set (value 0 is valid, e.g., for enum selections)
                let paramsArray = Array(slot.params).sorted { $0.key < $1.key }
                let numParams = min(paramsArray.count, 8)
                msg.append(UInt8(numParams) & 0x7F)

                // 8 param pairs (id, value) - always send 8 pairs
                for p in 0..<8 {
                    if p < paramsArray.count {
                        let param = paramsArray[p]
                        msg.append(param.key & 0x7F)
                        msg.append(param.value & 0x7F)
                    } else {
                        // Fill with zeros for unused param slots
                        msg.append(0)
                        msg.append(0)
                    }
                }
            } else {
                // Empty slot - 26 bytes of zeros
                msg.append(UInt8(i) & 0x7F)  // slotIndex
                msg.append(contentsOf: [UInt8](repeating: 0, count: 25))
            }
        }

        // 2 button mappings (unassigned)
        msg.append(127)  // button 0 slotIndex (127 = unassigned)
        msg.append(0)  // button 0 mode
        msg.append(127)  // button 1 slotIndex
        msg.append(0)  // button 1 mode

        // Input gain (Q16.16, 5 bytes)
        msg.append(contentsOf: packQ16_16(floatToQ16_16(patch.inputGainDb)))

        // Output gain (Q16.16, 5 bytes)
        msg.append(contentsOf: packQ16_16(floatToQ16_16(patch.outputGainDb)))

        msg.append(0xF7)
        return msg
    }

    // MARK: - Q16.16 Encoders

    /// Convert Float to Q16.16 fixed-point
    static func floatToQ16_16(_ v: Float) -> Int32 {
        return Int32(v * 65536.0 + 0.5)
    }

    /// Pack Q16.16 value into 5 bytes (7-bit safe)
    static func packQ16_16(_ value: Int32) -> [UInt8] {
        let u = UInt32(bitPattern: value)
        return [
            UInt8(u & 0x7F),
            UInt8((u >> 7) & 0x7F),
            UInt8((u >> 14) & 0x7F),
            UInt8((u >> 21) & 0x7F),
            UInt8((u >> 28) & 0x7F),
        ]
    }

    // MARK: - Q16.16 Decoders

    /// Unpack 5 bytes (7-bit safe) into Q16.16 fixed-point value
    static func unpackQ16_16(_ bytes: ArraySlice<UInt8>) -> Int32 {
        guard bytes.count >= 5 else { return 0 }
        let b = Array(bytes)
        var u: UInt32 = 0
        u |= UInt32(b[0] & 0x7F)
        u |= UInt32(b[1] & 0x7F) << 7
        u |= UInt32(b[2] & 0x7F) << 14
        u |= UInt32(b[3] & 0x7F) << 21
        u |= UInt32(b[4] & 0x7F) << 28
        return Int32(bitPattern: u)
    }

    /// Convert Q16.16 fixed-point to Float
    static func q16_16ToFloat(_ v: Int32) -> Float {
        return Float(v) / 65536.0
    }
}
