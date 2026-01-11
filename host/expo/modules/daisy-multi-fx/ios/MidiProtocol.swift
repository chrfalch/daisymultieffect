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
        static let setParam: UInt8 = 0x20
        static let setEnabled: UInt8 = 0x21
        static let setType: UInt8 = 0x22
        static let setRouting: UInt8 = 0x23
        static let setSumToMono: UInt8 = 0x24
        static let setMix: UInt8 = 0x25
        static let setChannelPolicy: UInt8 = 0x26
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
