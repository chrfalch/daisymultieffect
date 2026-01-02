import Foundation

/// MIDI Protocol definitions matching C++ MidiProtocol
enum MidiProtocol {
    static let manufacturerId: UInt8 = 0x7D

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
        static let requestMeta: UInt8 = 0x32
    }

    enum Resp {
        static let patchDump: UInt8 = 0x13
        static let effectMeta: UInt8 = 0x33
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
}
