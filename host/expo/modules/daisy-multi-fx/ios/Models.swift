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
    var channelPolicy: UInt8 = 0
    var params: [UInt8: UInt8] = [:]

    init() {}

    init(from dict: [String: Any]) {
        slotIndex = UInt8(dict["slotIndex"] as? Int ?? 0)
        typeId = UInt8(dict["typeId"] as? Int ?? 0)
        enabled = dict["enabled"] as? Bool ?? false
        inputL = UInt8(dict["inputL"] as? Int ?? 0)
        inputR = UInt8(dict["inputR"] as? Int ?? 0)
        sumToMono = dict["sumToMono"] as? Bool ?? false
        dry = UInt8(dict["dry"] as? Int ?? 0)
        wet = UInt8(dict["wet"] as? Int ?? 127)
        channelPolicy = UInt8(dict["channelPolicy"] as? Int ?? 0)
        if let paramsDict = dict["params"] as? [String: Int] {
            for (key, value) in paramsDict {
                if let keyInt = UInt8(key) {
                    params[keyInt] = UInt8(value)
                }
            }
        }
    }

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
            "channelPolicy": Int(channelPolicy),
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
    var inputGainDb: Float = 18.0  // Default +18dB for instrument level
    var outputGainDb: Float = 0.0  // Default 0dB (unity)

    init() {}

    init(from dict: [String: Any]) {
        numSlots = UInt8(dict["numSlots"] as? Int ?? 0)
        inputGainDb = Float(dict["inputGainDb"] as? Double ?? 18.0)
        outputGainDb = Float(dict["outputGainDb"] as? Double ?? 0.0)
        if let slotsArray = dict["slots"] as? [[String: Any]] {
            slots = slotsArray.map { EffectSlot(from: $0) }
        }
    }

    func toDictionary() -> [String: Any] {
        return [
            "numSlots": Int(numSlots),
            "slots": slots.map { $0.toDictionary() },
            "inputGainDb": Double(inputGainDb),
            "outputGainDb": Double(outputGainDb),
        ]
    }
}

// MARK: - Effect Metadata

/// Number parameter range metadata
struct NumberRangeMeta: Equatable {
    var min: Float
    var max: Float
    var step: Float

    func toDictionary() -> [String: Any] {
        return [
            "min": Double(min),
            "max": Double(max),
            "step": Double(step),
        ]
    }
}

/// Enum option for dropdown parameters
struct EnumOption: Equatable {
    var value: UInt8
    var name: String

    func toDictionary() -> [String: Any] {
        return [
            "value": Int(value),
            "name": name,
        ]
    }
}

/// Effect parameter metadata
struct EffectParamMeta: Equatable {
    var id: UInt8
    var name: String
    var kind: UInt8
    var range: NumberRangeMeta?
    var enumOptions: [EnumOption]?
    var description: String?
    var unitPrefix: String?
    var unitSuffix: String?
    var isDisplayParam: Bool = false

    func toDictionary() -> [String: Any] {
        var dict: [String: Any] = [
            "id": Int(id),
            "name": name,
            "kind": Int(kind),
        ]
        if let range {
            dict["range"] = range.toDictionary()
        }
        if let enumOptions, !enumOptions.isEmpty {
            dict["enumOptions"] = enumOptions.map { $0.toDictionary() }
        }
        if let description, !description.isEmpty {
            dict["description"] = description
        }
        if let unitPrefix, !unitPrefix.isEmpty {
            dict["unitPrefix"] = unitPrefix
        }
        if let unitSuffix, !unitSuffix.isEmpty {
            dict["unitSuffix"] = unitSuffix
        }
        if isDisplayParam {
            dict["isDisplayParam"] = true
        }
        return dict
    }
}

/// Effect metadata
struct EffectMeta: Equatable {
    var typeId: UInt8
    var name: String
    var shortName: String  // 3-character short name
    var description: String?
    var params: [EffectParamMeta]

    func toDictionary() -> [String: Any] {
        var dict: [String: Any] = [
            "typeId": Int(typeId),
            "name": name,
            "shortName": shortName,
            "params": params.map { $0.toDictionary() },
        ]
        if let description, !description.isEmpty {
            dict["description"] = description
        }
        return dict
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

// MARK: - Device Status

/// Device status with audio levels and CPU load
struct DeviceStatus {
    /// Input level (linear, 0.0-1.0+)
    var inputLevel: Float
    /// Output level (linear, 0.0-1.0+)
    var outputLevel: Float
    /// Average CPU load (0.0-1.0)
    var cpuAvg: Float
    /// Maximum CPU load since last reset (0.0-1.0)
    var cpuMax: Float

    func toDictionary() -> [String: Any] {
        return [
            "inputLevel": inputLevel,
            "outputLevel": outputLevel,
            "cpuAvg": cpuAvg,
            "cpuMax": cpuMax,
        ]
    }
}
