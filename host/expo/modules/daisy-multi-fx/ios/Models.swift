import Foundation

// MARK: - Effect Slot

/// Effect slot state
struct EffectSlot: Equatable {
    var slotIndex: UInt8 = 0
    var typeId: UInt8 = 0
    var enabled: Bool = false
    var inputL: UInt8 = 0
    var inputR: UInt8 = 0
    var sumToMono: Bool = false
    var dry: UInt8 = 0
    var wet: UInt8 = 127
    var params: [UInt8: UInt8] = [:]

    func toDictionary() -> [String: Any] {
        return [
            "slotIndex": Int(slotIndex),
            "typeId": Int(typeId),
            "enabled": enabled,
            "inputL": Int(inputL),
            "inputR": Int(inputR),
            "sumToMono": sumToMono,
            "dry": Int(dry),
            "wet": Int(wet),
            "params": params.reduce(into: [String: Int]()) { result, pair in
                result[String(pair.key)] = Int(pair.value)
            },
        ]
    }
}

// MARK: - Patch

/// Complete patch state
struct Patch: Equatable {
    var numSlots: UInt8 = 0
    var slots: [EffectSlot] = []

    func toDictionary() -> [String: Any] {
        return [
            "numSlots": Int(numSlots),
            "slots": slots.map { $0.toDictionary() },
        ]
    }
}

// MARK: - Effect Metadata

/// Effect parameter metadata
struct EffectParamMeta: Equatable {
    var id: UInt8
    var name: String

    func toDictionary() -> [String: Any] {
        return [
            "id": Int(id),
            "name": name,
        ]
    }
}

/// Effect metadata
struct EffectMeta: Equatable {
    var typeId: UInt8
    var name: String
    var shortName: String  // 3-character short name
    var params: [EffectParamMeta]

    func toDictionary() -> [String: Any] {
        return [
            "typeId": Int(typeId),
            "name": name,
            "shortName": shortName,
            "params": params.map { $0.toDictionary() },
        ]
    }
}

// MARK: - Connection Status

/// MIDI connection status
struct ConnectionStatus {
    var connected: Bool
    var sourceCount: Int
    var sourceNames: [String]
    var destinationName: String
    var status: String

    func toDictionary() -> [String: Any] {
        return [
            "connected": connected,
            "sourceCount": sourceCount,
            "sourceNames": sourceNames,
            "destinationName": destinationName,
            "status": status,
        ]
    }
}
