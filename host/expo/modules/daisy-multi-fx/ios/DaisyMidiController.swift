import CoreMIDI
import Foundation

// MARK: - Thread-safe SysEx Accumulator

private final class SysExAccumulator: @unchecked Sendable {
    private var buffer: [UInt8] = []
    private var isAccumulating = false
    private let lock = NSLock()

    func process(status: UInt8, payload: [UInt8]) -> [UInt8]? {
        lock.lock()
        defer { lock.unlock() }

        switch status {
        case 0x00:
            return payload
        case 0x01:
            buffer = payload
            isAccumulating = true
            return nil
        case 0x02:
            if isAccumulating {
                buffer.append(contentsOf: payload)
            }
            return nil
        case 0x03:
            if isAccumulating {
                buffer.append(contentsOf: payload)
                let complete = buffer
                buffer = []
                isAccumulating = false
                return complete
            }
            return nil
        default:
            return nil
        }
    }

    func reset() {
        lock.lock()
        defer { lock.unlock() }
        buffer = []
        isAccumulating = false
    }
}

// MARK: - MIDI Controller

/// Singleton MIDI controller that manages CoreMIDI connections
final class DaisyMidiController: @unchecked Sendable {
    static let shared = DaisyMidiController()

    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var outPort = MIDIPortRef()
    private var destination: MIDIEndpointRef = 0

    private var isSetup = false
    private let lock = NSLock()
    private let sysexAccumulator = SysExAccumulator()

    // State
    private(set) var patch: Patch?
    private(set) var effectsMeta: [EffectMeta] = []
    private(set) var connectionStatus: ConnectionStatus = ConnectionStatus(
        connected: false,
        sourceCount: 0,
        sourceNames: [],
        destinationName: "",
        status: "Not initialized"
    )

    // Callbacks (set by the module)
    var onPatchUpdate: ((Patch) -> Void)?
    var onEffectMetaUpdate: (([EffectMeta]) -> Void)?
    var onConnectionStatusChange: ((ConnectionStatus) -> Void)?
    var onStatusUpdate: ((DeviceStatus) -> Void)?

    private init() {}

    // MARK: - Setup / Teardown

    func setup() {
        lock.lock()
        defer { lock.unlock() }

        guard !isSetup else {
            NSLog("[DaisyMidi] Already setup")
            return
        }

        isSetup = true
        setupCoreMidi()
    }

    func teardown() {
        lock.lock()
        defer { lock.unlock() }

        guard isSetup else { return }

        if inPort != 0 {
            MIDIPortDispose(inPort)
            inPort = 0
        }
        if outPort != 0 {
            MIDIPortDispose(outPort)
            outPort = 0
        }
        if client != 0 {
            MIDIClientDispose(client)
            client = 0
        }

        destination = 0
        isSetup = false
        patch = nil
        effectsMeta = []

        updateConnectionStatus(
            ConnectionStatus(
                connected: false,
                sourceCount: 0,
                sourceNames: [],
                destinationName: "",
                status: "Disconnected"
            ))
    }

    // MARK: - CoreMIDI Setup

    private func setupCoreMidi() {
        // Enable network MIDI session (required for MIDI over network on iOS)
        let networkSession = MIDINetworkSession.default()
        networkSession.isEnabled = true
        networkSession.connectionPolicy = .anyone
        NSLog(
            "[DaisyMidi] Network MIDI enabled: \(networkSession.isEnabled), policy: \(networkSession.connectionPolicy.rawValue)"
        )

        // Create client with notification callback for device changes
        let status = MIDIClientCreateWithBlock("DaisyMultiFX" as CFString, &client) {
            [weak self] notification in
            self?.handleMidiNotification(notification)
        }
        guard status == noErr else {
            updateConnectionStatus(
                ConnectionStatus(
                    connected: false,
                    sourceCount: 0,
                    sourceNames: [],
                    destinationName: "",
                    status: "MIDI client creation failed: \(status)"
                ))
            return
        }

        // Create input port
        let inputStatus = MIDIInputPortCreateWithProtocol(
            client,
            "Input" as CFString,
            ._1_0,
            &inPort
        ) { [weak self] eventList, _ in
            self?.handleMidiEventList(eventList)
        }

        guard inputStatus == noErr else {
            updateConnectionStatus(
                ConnectionStatus(
                    connected: false,
                    sourceCount: 0,
                    sourceNames: [],
                    destinationName: "",
                    status: "MIDI input port failed: \(inputStatus)"
                ))
            return
        }

        // Create output port
        MIDIOutputPortCreate(client, "Output" as CFString, &outPort)

        // Log all available sources
        let sourceCount = MIDIGetNumberOfSources()
        NSLog("[DaisyMidi] Found \(sourceCount) MIDI sources:")
        var sourceNames: [String] = []
        for i in 0..<sourceCount {
            let src = MIDIGetSource(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(src, kMIDIPropertyName, &name)
            let srcName = (name?.takeRetainedValue() as String?) ?? "Unknown"
            sourceNames.append(srcName)
            NSLog("[DaisyMidi]   Source \(i): \(srcName)")
            MIDIPortConnectSource(inPort, src, nil)
        }

        // Also connect to the network session source if available
        let networkSourceEndpoint = networkSession.sourceEndpoint()
        if networkSourceEndpoint != 0 {
            NSLog("[DaisyMidi] Connecting to network session source endpoint")
            MIDIPortConnectSource(inPort, networkSourceEndpoint, nil)
        }

        // Find destination - log all available
        let destCount = MIDIGetNumberOfDestinations()
        NSLog("[DaisyMidi] Found \(destCount) MIDI destinations:")
        var destNames: [String] = []
        var selectedDestName = ""

        for i in 0..<destCount {
            let dest = MIDIGetDestination(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            if let n = name?.takeRetainedValue() as String? {
                destNames.append(n)
                NSLog("[DaisyMidi]   Destination \(i): \(n)")
                // Match network MIDI, IAC, or Daisy
                if destination == 0
                    && (n.contains("IAC") || n.contains("Bus") || n.contains("Daisy")
                        || n.contains("Network") || n.contains("Session"))
                {
                    destination = dest
                    selectedDestName = n
                    NSLog("[DaisyMidi] Selected destination: \(n)")
                }
            }
        }

        // Also check for network session destination
        let networkDestEndpoint = networkSession.destinationEndpoint()
        if networkDestEndpoint != 0 && destination == 0 {
            destination = networkDestEndpoint
            selectedDestName = "Network Session"
            NSLog("[DaisyMidi] Using network session destination endpoint")
        }

        if destination == 0 && destCount > 0 {
            destination = MIDIGetDestination(0)
            selectedDestName = destNames.first ?? "unknown"
            NSLog("[DaisyMidi] Fallback to first destination: \(selectedDestName)")
        }

        updateConnectionStatus(
            ConnectionStatus(
                connected: destination != 0,
                sourceCount: Int(sourceCount),
                sourceNames: sourceNames,
                destinationName: selectedDestName,
                status: "MIDI ready - \(sourceNames.joined(separator: ", "))"
            ))
    }

    // MARK: - MIDI Notifications

    private func handleMidiNotification(_ notification: UnsafePointer<MIDINotification>) {
        let messageID = notification.pointee.messageID

        switch messageID {
        case .msgSetupChanged:
            NSLog("[DaisyMidi] MIDI setup changed - rescanning devices")
            DispatchQueue.main.async { [weak self] in
                self?.rescanMidiDevices()
            }

        case .msgObjectAdded:
            NSLog("[DaisyMidi] MIDI object added")
            DispatchQueue.main.async { [weak self] in
                self?.rescanMidiDevices()
            }

        case .msgObjectRemoved:
            NSLog("[DaisyMidi] MIDI object removed")
            DispatchQueue.main.async { [weak self] in
                self?.rescanMidiDevices()
            }

        case .msgPropertyChanged:
            NSLog("[DaisyMidi] MIDI property changed")

        case .msgThruConnectionsChanged:
            NSLog("[DaisyMidi] MIDI thru connections changed")

        case .msgSerialPortOwnerChanged:
            NSLog("[DaisyMidi] Serial port owner changed")

        case .msgIOError:
            NSLog("[DaisyMidi] MIDI I/O error")

        @unknown default:
            NSLog("[DaisyMidi] Unknown MIDI notification: \(messageID.rawValue)")
        }
    }

    private func rescanMidiDevices() {
        guard isSetup, inPort != 0 else { return }

        let networkSession = MIDINetworkSession.default()

        // Reconnect to all sources
        let sourceCount = MIDIGetNumberOfSources()
        NSLog("[DaisyMidi] Rescanning - found \(sourceCount) MIDI sources:")
        var sourceNames: [String] = []

        for i in 0..<sourceCount {
            let src = MIDIGetSource(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(src, kMIDIPropertyName, &name)
            let srcName = (name?.takeRetainedValue() as String?) ?? "Unknown"
            sourceNames.append(srcName)
            NSLog("[DaisyMidi]   Source \(i): \(srcName)")
            MIDIPortConnectSource(inPort, src, nil)
        }

        // Connect to network session source
        let networkSourceEndpoint = networkSession.sourceEndpoint()
        if networkSourceEndpoint != 0 {
            MIDIPortConnectSource(inPort, networkSourceEndpoint, nil)
        }

        // Rescan destinations
        let destCount = MIDIGetNumberOfDestinations()
        NSLog("[DaisyMidi] Rescanning - found \(destCount) MIDI destinations:")
        var selectedDestName = ""
        var newDestination: MIDIEndpointRef = 0

        for i in 0..<destCount {
            let dest = MIDIGetDestination(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            if let n = name?.takeRetainedValue() as String? {
                NSLog("[DaisyMidi]   Destination \(i): \(n)")
                if newDestination == 0
                    && (n.contains("IAC") || n.contains("Bus") || n.contains("Daisy")
                        || n.contains("Network") || n.contains("Session"))
                {
                    newDestination = dest
                    selectedDestName = n
                }
            }
        }

        // Check network session destination
        let networkDestEndpoint = networkSession.destinationEndpoint()
        if networkDestEndpoint != 0 && newDestination == 0 {
            newDestination = networkDestEndpoint
            selectedDestName = "Network Session"
        }

        if newDestination == 0 && destCount > 0 {
            newDestination = MIDIGetDestination(0)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(newDestination, kMIDIPropertyName, &name)
            selectedDestName = (name?.takeRetainedValue() as String?) ?? "unknown"
        }

        destination = newDestination

        updateConnectionStatus(
            ConnectionStatus(
                connected: destination != 0,
                sourceCount: Int(sourceCount),
                sourceNames: sourceNames,
                destinationName: selectedDestName,
                status: "MIDI ready - \(sourceNames.joined(separator: ", "))"
            ))
    }

    private func updateConnectionStatus(_ newStatus: ConnectionStatus) {
        connectionStatus = newStatus
        DispatchQueue.main.async { [weak self] in
            self?.onConnectionStatusChange?(newStatus)
        }
    }

    // MARK: - MIDI Event Handling

    private func handleMidiEventList(_ eventList: UnsafePointer<MIDIEventList>) {
        guard eventList.pointee.numPackets > 0 else { return }

        var packet: UnsafePointer<MIDIEventPacket> = UnsafePointer(
            UnsafeRawPointer(eventList).advanced(
                by: MemoryLayout<MIDIEventList>.offset(of: \MIDIEventList.packet)!
            )
            .assumingMemoryBound(to: MIDIEventPacket.self)
        )

        for _ in 0..<Int(eventList.pointee.numPackets) {
            let wordCount = Int(packet.pointee.wordCount)

            if wordCount > 0 {
                let byteCount = wordCount * 4
                var bytes = [UInt8](repeating: 0, count: byteCount)

                let wordsOffset = MemoryLayout<MIDITimeStamp>.size + MemoryLayout<UInt32>.size
                let wordsPtr = UnsafeRawPointer(packet).advanced(by: wordsOffset)
                for i in 0..<byteCount {
                    bytes[i] = wordsPtr.load(fromByteOffset: i, as: UInt8.self)
                }

                if let sysex = extractSysex(from: bytes) {
                    DispatchQueue.main.async { [weak self] in
                        self?.handleSysex(sysex)
                    }
                }
            }

            packet = UnsafePointer(MIDIEventPacketNext(packet))
        }
    }

    private func extractSysex(from umpData: [UInt8]) -> [UInt8]? {
        guard umpData.count >= 4 else { return nil }

        let msgTypeByte = umpData[3]
        let messageType = (msgTypeByte >> 4) & 0x0F

        // Message type 3 = Data Messages (SysEx)
        if messageType == 0x03 {
            // Process each 64-bit packet within this event
            var offset = 0

            while offset + 8 <= umpData.count {
                let statusByte = umpData[offset + 2]
                let status = (statusByte >> 4) & 0x0F  // 0=complete, 1=start, 2=continue, 3=end
                let numBytes = Int(statusByte & 0x0F)

                // Extract data bytes from this 64-bit packet
                var packetData: [UInt8] = []
                if numBytes >= 1 { packetData.append(umpData[offset + 1]) }
                if numBytes >= 2 { packetData.append(umpData[offset + 0]) }
                if numBytes >= 3 && offset + 7 < umpData.count {
                    packetData.append(umpData[offset + 7])
                }
                if numBytes >= 4 && offset + 6 < umpData.count {
                    packetData.append(umpData[offset + 6])
                }
                if numBytes >= 5 && offset + 5 < umpData.count {
                    packetData.append(umpData[offset + 5])
                }
                if numBytes >= 6 && offset + 4 < umpData.count {
                    packetData.append(umpData[offset + 4])
                }

                // Use the accumulator for multi-packet SysEx
                if let completeSysex = sysexAccumulator.process(status: status, payload: packetData)
                {
                    NSLog("[DaisyMidi] Accumulated complete SysEx: \(completeSysex.count) bytes")
                    return completeSysex
                }

                // Check if we should stop (padding/invalid)
                if status == 0x00 && numBytes == 0 {
                    break
                }

                offset += 8
            }

            // SysEx not complete yet (waiting for more packets)
            return nil
        }

        // Legacy format (direct F0...F7)
        if umpData[0] == 0xF0 {
            if let endIndex = umpData.firstIndex(of: 0xF7) {
                return Array(umpData[1..<endIndex])
            }
        }

        return nil
    }

    private func handleSysex(_ data: [UInt8]) {
        guard data.count >= 3, data[0] == MidiProtocol.manufacturerId else { return }

        let sender = data[1]
        let cmd = data[2]

        // Ignore our own messages
        if sender == MidiProtocol.Sender.swift { return }

        NSLog("[DaisyMidi] Received SysEx cmd=0x\(String(format: "%02X", cmd))")

        switch cmd {
        case MidiProtocol.Resp.patchDump:
            NSLog("[DaisyMidi] Decoding PATCH_DUMP, data size=\(data.count)")
            if let newPatch = decodePatchDump(data) {
                NSLog("[DaisyMidi] Patch decoded: \(newPatch.numSlots) slots")
                patch = newPatch
                onPatchUpdate?(newPatch)
            } else {
                NSLog("[DaisyMidi] PATCH_DUMP decode FAILED")
            }

        case MidiProtocol.Resp.effectMeta:
            let effects = decodeEffectMeta(data)
            effectsMeta = effects
            onEffectMetaUpdate?(effects)

        case MidiProtocol.Resp.effectMetaV3:
            // Single effect V3 metadata with short name
            if let effect = decodeEffectMetaV3(data) {
                // Update or add to effectsMeta
                if let idx = effectsMeta.firstIndex(where: { $0.typeId == effect.typeId }) {
                    effectsMeta[idx] = effect
                } else {
                    effectsMeta.append(effect)
                }
                onEffectMetaUpdate?(effectsMeta)
            }

        case MidiProtocol.Resp.effectMetaV4:
            // Single effect V4 metadata with short name + param ranges
            if let effect = decodeEffectMetaV4(data) {
                if let idx = effectsMeta.firstIndex(where: { $0.typeId == effect.typeId }) {
                    effectsMeta[idx] = effect
                } else {
                    effectsMeta.append(effect)
                }
                onEffectMetaUpdate?(effectsMeta)
            }

        case MidiProtocol.Resp.effectMetaV5:
            // Single effect V5 metadata with descriptions + units + optional ranges
            NSLog("[DaisyMidi] Decoding effectMetaV5, data size=\(data.count)")
            if let effect = decodeEffectMetaV5(data) {
                NSLog(
                    "[DaisyMidi] V5 decoded: \(effect.name) (typeId=\(effect.typeId), params=\(effect.params.count))"
                )
                if let idx = effectsMeta.firstIndex(where: { $0.typeId == effect.typeId }) {
                    effectsMeta[idx] = effect
                } else {
                    effectsMeta.append(effect)
                }
                NSLog("[DaisyMidi] effectsMeta now has \(effectsMeta.count) effects")
                onEffectMetaUpdate?(effectsMeta)
            } else {
                NSLog(
                    "[DaisyMidi] V5 decode FAILED for data: \(data.prefix(20).map { String(format: "%02X", $0) }.joined(separator: " "))"
                )
            }

        case MidiProtocol.Cmd.setEnabled:
            if data.count >= 5 {
                let slot = data[3]
                let enabled = data[4] != 0
                updateSlotEnabled(slot: slot, enabled: enabled)
            }

        case MidiProtocol.Cmd.setType:
            if data.count >= 5 {
                let slot = data[3]
                let typeId = data[4]
                updateSlotType(slot: slot, typeId: typeId)
            }

        case MidiProtocol.Cmd.setParam:
            if data.count >= 6 {
                let slot = data[3]
                let paramId = data[4]
                let value = data[5]
                updateSlotParam(slot: slot, paramId: paramId, value: value)
            }

        case MidiProtocol.Cmd.setRouting:
            // Data format (with F0/F7 stripped): 7D <sender> <cmd> <slot> <inputL> <inputR>
            if data.count >= 6 {
                let slot = data[3]
                let inputL = MidiProtocol.decodeRoute(data[4])
                let inputR = MidiProtocol.decodeRoute(data[5])
                updateSlotRouting(slot: slot, inputL: inputL, inputR: inputR)
            }

        case MidiProtocol.Cmd.setSumToMono:
            // 7D <sender> <cmd> <slot> <sumToMono>
            if data.count >= 5 {
                let slot = data[3]
                let sumToMono = data[4] != 0
                updateSlotSumToMono(slot: slot, sumToMono: sumToMono)
            }

        case MidiProtocol.Cmd.setMix:
            // 7D <sender> <cmd> <slot> <dry> <wet>
            if data.count >= 6 {
                let slot = data[3]
                let dry = data[4]
                let wet = data[5]
                updateSlotMix(slot: slot, dry: dry, wet: wet)
            }

        case MidiProtocol.Cmd.setChannelPolicy:
            // 7D <sender> <cmd> <slot> <policy>
            if data.count >= 5 {
                let slot = data[3]
                let policy = data[4]
                updateSlotChannelPolicy(slot: slot, channelPolicy: policy)
            }

        case MidiProtocol.Resp.statusUpdate:
            // 7D <sender> 42 [inputLevel 5B] [outputLevel 5B] [cpuAvg 5B] [cpuMax 5B]
            // Total: 3 header + 4*5 values = 23 bytes
            if data.count >= 23 {
                let inputLevel = MidiProtocol.q16_16ToFloat(MidiProtocol.unpackQ16_16(data[3..<8]))
                let outputLevel = MidiProtocol.q16_16ToFloat(
                    MidiProtocol.unpackQ16_16(data[8..<13]))
                let cpuAvg = MidiProtocol.q16_16ToFloat(MidiProtocol.unpackQ16_16(data[13..<18]))
                let cpuMax = MidiProtocol.q16_16ToFloat(MidiProtocol.unpackQ16_16(data[18..<23]))

                let status = DeviceStatus(
                    inputLevel: inputLevel,
                    outputLevel: outputLevel,
                    cpuAvg: cpuAvg,
                    cpuMax: cpuMax
                )
                onStatusUpdate?(status)
            }

        default:
            break
        }
    }

    // MARK: - Decoders

    private func decodePatchDump(_ data: [UInt8]) -> Patch? {
        guard data.count > 4 else { return nil }

        var newPatch = Patch()
        newPatch.numSlots = data[3]

        var offset = 4
        for _ in 0..<12 {
            guard offset + 26 <= data.count else { break }

            var slot = EffectSlot()
            slot.slotIndex = data[offset]
            offset += 1
            slot.typeId = data[offset]
            offset += 1
            slot.enabled = data[offset] != 0
            offset += 1
            slot.inputL = MidiProtocol.decodeRoute(data[offset])
            offset += 1
            slot.inputR = MidiProtocol.decodeRoute(data[offset])
            offset += 1
            slot.sumToMono = data[offset] != 0
            offset += 1
            slot.dry = data[offset]
            offset += 1
            slot.wet = data[offset]
            offset += 1
            slot.channelPolicy = data[offset]
            offset += 1
            let numParams = data[offset]
            offset += 1

            for p in 0..<8 {
                let paramId = data[offset]
                offset += 1
                let paramVal = data[offset]
                offset += 1
                if p < numParams {
                    slot.params[paramId] = paramVal
                }
            }

            newPatch.slots.append(slot)
        }

        // Parse button assignments (2x 2 bytes each = 4 bytes)
        offset += 4

        // Parse gain values if present (5 bytes each for Q16.16)
        if offset + 10 <= data.count {
            let inputGainQ16 = MidiProtocol.unpackQ16_16(data[offset..<(offset + 5)])
            newPatch.inputGainDb = MidiProtocol.q16_16ToFloat(inputGainQ16)
            offset += 5

            let outputGainQ16 = MidiProtocol.unpackQ16_16(data[offset..<(offset + 5)])
            newPatch.outputGainDb = MidiProtocol.q16_16ToFloat(outputGainQ16)
            offset += 5
        }

        return newPatch
    }

    private func decodeEffectMeta(_ data: [UInt8]) -> [EffectMeta] {
        guard data.count > 4 else { return [] }

        var effects: [EffectMeta] = []
        let numEffects = Int(data[3])
        var offset = 4

        for _ in 0..<numEffects {
            guard offset < data.count else { break }

            let typeId = data[offset]
            offset += 1
            guard offset < data.count else { break }

            let nameLen = Int(data[offset])
            offset += 1
            guard offset + nameLen <= data.count else { break }

            let name = String(bytes: data[offset..<(offset + nameLen)], encoding: .ascii) ?? "?"
            offset += nameLen

            // Default short name from first 3 chars of name
            let shortName = String(name.prefix(3)).uppercased()

            guard offset < data.count else { break }
            let numParams = Int(data[offset])
            offset += 1

            var params: [EffectParamMeta] = []
            for _ in 0..<numParams {
                guard offset < data.count else { break }
                let paramId = data[offset]
                offset += 1
                guard offset < data.count else { break }
                let paramNameLen = Int(data[offset])
                offset += 1
                guard offset + paramNameLen <= data.count else { break }
                let paramName =
                    String(bytes: data[offset..<(offset + paramNameLen)], encoding: .ascii) ?? "?"
                offset += paramNameLen
                params.append(
                    EffectParamMeta(
                        id: paramId,
                        name: paramName,
                        kind: 0,
                        range: nil,
                        description: nil,
                        unitPrefix: nil,
                        unitSuffix: nil
                    ))
            }

            effects.append(
                EffectMeta(
                    typeId: typeId, name: name, shortName: shortName, description: nil,
                    params: params))
        }

        return effects
    }

    /// Decode V3 effect metadata with short name
    /// Format: F0 7D 36 <typeId> <nameLen> <name...> <shortName[3]> <numParams> (paramId kind nameLen name...)xN F7
    private func decodeEffectMetaV3(_ data: [UInt8]) -> EffectMeta? {
        guard data.count > 3 else { return nil }

        var offset = 3  // Skip F0, 7D, 36

        guard offset < data.count else { return nil }
        let typeId = data[offset]
        offset += 1

        guard offset < data.count else { return nil }
        let nameLen = Int(data[offset])
        offset += 1

        guard offset + nameLen <= data.count else { return nil }
        let name = String(bytes: data[offset..<(offset + nameLen)], encoding: .ascii) ?? "?"
        offset += nameLen

        // Read 3-character short name
        guard offset + 3 <= data.count else { return nil }
        let shortName = String(bytes: data[offset..<(offset + 3)], encoding: .ascii) ?? "---"
        offset += 3

        guard offset < data.count else { return nil }
        let numParams = Int(data[offset])
        offset += 1

        var params: [EffectParamMeta] = []
        for _ in 0..<numParams {
            guard offset < data.count else { break }
            let paramId = data[offset]
            offset += 1

            // Skip 'kind' byte
            guard offset < data.count else { break }
            let kind = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let paramNameLen = Int(data[offset])
            offset += 1

            guard offset + paramNameLen <= data.count else { break }
            let paramName =
                String(bytes: data[offset..<(offset + paramNameLen)], encoding: .ascii) ?? "?"
            offset += paramNameLen

            params.append(
                EffectParamMeta(
                    id: paramId,
                    name: paramName,
                    kind: kind,
                    range: nil,
                    description: nil,
                    unitPrefix: nil,
                    unitSuffix: nil
                ))
        }

        return EffectMeta(
            typeId: typeId, name: name, shortName: shortName, description: nil, params: params)
    }

    /// Decode V4 effect metadata with short name and number param ranges.
    /// Format: F0 7D <sender> 37 <typeId> <nameLen> <name...> <shortName[3]> <numParams>
    ///   (paramId kind flags nameLen name... [minQ16.16_5] [maxQ16.16_5] [stepQ16.16_5])xN F7
    private func decodeEffectMetaV4(_ data: [UInt8]) -> EffectMeta? {
        guard data.count > 3 else { return nil }

        var offset = 3  // Skip manufacturer, sender, cmd

        guard offset < data.count else { return nil }
        let typeId = data[offset]
        offset += 1

        guard offset < data.count else { return nil }
        let nameLen = Int(data[offset])
        offset += 1

        guard offset + nameLen <= data.count else { return nil }
        let name = String(bytes: data[offset..<(offset + nameLen)], encoding: .ascii) ?? "?"
        offset += nameLen

        guard offset + 3 <= data.count else { return nil }
        let shortName = String(bytes: data[offset..<(offset + 3)], encoding: .ascii) ?? "---"
        offset += 3

        guard offset < data.count else { return nil }
        let numParams = Int(data[offset])
        offset += 1

        var params: [EffectParamMeta] = []
        params.reserveCapacity(numParams)

        for _ in 0..<numParams {
            guard offset < data.count else { break }
            let paramId = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let kind = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let flags = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let paramNameLen = Int(data[offset])
            offset += 1

            guard offset + paramNameLen <= data.count else { break }
            let paramName =
                String(bytes: data[offset..<(offset + paramNameLen)], encoding: .ascii) ?? "?"
            offset += paramNameLen

            var range: NumberRangeMeta? = nil
            let hasRange = (flags & 0x01) != 0
            if hasRange {
                guard offset + 15 <= data.count else { break }
                let minVal = unpackQ16_16(data, offset: offset)
                offset += 5
                let maxVal = unpackQ16_16(data, offset: offset)
                offset += 5
                let stepVal = unpackQ16_16(data, offset: offset)
                offset += 5
                range = NumberRangeMeta(min: minVal, max: maxVal, step: stepVal)
            }

            params.append(
                EffectParamMeta(
                    id: paramId,
                    name: paramName,
                    kind: kind,
                    range: range,
                    description: nil,
                    unitPrefix: nil,
                    unitSuffix: nil
                ))
        }

        return EffectMeta(
            typeId: typeId, name: name, shortName: shortName, description: nil, params: params)
    }

    private func unpackQ16_16(_ data: [UInt8], offset: Int) -> Float {
        let u: UInt32 =
            (UInt32(data[offset + 0]) & 0x7F)
            | ((UInt32(data[offset + 1]) & 0x7F) << 7)
            | ((UInt32(data[offset + 2]) & 0x7F) << 14)
            | ((UInt32(data[offset + 3]) & 0x7F) << 21)
            | ((UInt32(data[offset + 4]) & 0x7F) << 28)
        let i = Int32(bitPattern: u)
        return Float(i) / 65536.0
    }

    /// Decode V5 effect metadata with short name, param descriptions, and units.
    /// Format: F0 7D <sender> 38 <typeId> <nameLen> <name...> <shortName[3]> <effectDescLen> <effectDesc...> <numParams>
    ///   (paramId kind flags nameLen name... descLen desc... unitPreLen unitPre... unitSufLen unitSuf...
    ///    [minQ16.16_5] [maxQ16.16_5] [stepQ16.16_5])xN F7
    private func decodeEffectMetaV5(_ data: [UInt8]) -> EffectMeta? {
        guard data.count > 3 else { return nil }

        var offset = 3  // Skip manufacturer, sender, cmd

        guard offset < data.count else { return nil }
        let typeId = data[offset]
        offset += 1

        guard offset < data.count else { return nil }
        let nameLen = Int(data[offset])
        offset += 1

        guard offset + nameLen <= data.count else { return nil }
        let name = String(bytes: data[offset..<(offset + nameLen)], encoding: .ascii) ?? "?"
        offset += nameLen

        guard offset + 3 <= data.count else { return nil }
        let shortName = String(bytes: data[offset..<(offset + 3)], encoding: .ascii) ?? "---"
        offset += 3

        guard offset < data.count else { return nil }
        let effectDescLen = Int(data[offset])
        offset += 1
        guard offset + effectDescLen <= data.count else { return nil }
        let effectDesc =
            String(bytes: data[offset..<(offset + effectDescLen)], encoding: .ascii) ?? ""
        offset += effectDescLen

        guard offset < data.count else { return nil }
        let numParams = Int(data[offset])
        offset += 1

        var params: [EffectParamMeta] = []
        params.reserveCapacity(numParams)

        for _ in 0..<numParams {
            guard offset < data.count else { break }
            let paramId = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let kind = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let flags = data[offset]
            offset += 1

            guard offset < data.count else { break }
            let paramNameLen = Int(data[offset])
            offset += 1
            guard offset + paramNameLen <= data.count else { break }
            let paramName =
                String(bytes: data[offset..<(offset + paramNameLen)], encoding: .ascii) ?? "?"
            offset += paramNameLen

            guard offset < data.count else { break }
            let descLen = Int(data[offset])
            offset += 1
            guard offset + descLen <= data.count else { break }
            let desc =
                String(bytes: data[offset..<(offset + descLen)], encoding: .ascii) ?? ""
            offset += descLen

            guard offset < data.count else { break }
            let unitPreLen = Int(data[offset])
            offset += 1
            guard offset + unitPreLen <= data.count else { break }
            let unitPrefix =
                String(bytes: data[offset..<(offset + unitPreLen)], encoding: .ascii) ?? ""
            offset += unitPreLen

            guard offset < data.count else { break }
            let unitSufLen = Int(data[offset])
            offset += 1
            guard offset + unitSufLen <= data.count else { break }
            let unitSuffix =
                String(bytes: data[offset..<(offset + unitSufLen)], encoding: .ascii) ?? ""
            offset += unitSufLen

            var range: NumberRangeMeta? = nil
            let hasRange = (flags & 0x01) != 0
            if hasRange {
                guard offset + 15 <= data.count else { break }
                let minVal = unpackQ16_16(data, offset: offset)
                offset += 5
                let maxVal = unpackQ16_16(data, offset: offset)
                offset += 5
                let stepVal = unpackQ16_16(data, offset: offset)
                offset += 5
                range = NumberRangeMeta(min: minVal, max: maxVal, step: stepVal)
            }

            params.append(
                EffectParamMeta(
                    id: paramId,
                    name: paramName,
                    kind: kind,
                    range: range,
                    description: desc.isEmpty ? nil : desc,
                    unitPrefix: unitPrefix.isEmpty ? nil : unitPrefix,
                    unitSuffix: unitSuffix.isEmpty ? nil : unitSuffix
                ))
        }

        return EffectMeta(
            typeId: typeId,
            name: name,
            shortName: shortName,
            description: effectDesc.isEmpty ? nil : effectDesc,
            params: params
        )
    }

    // MARK: - State Updates (from incoming MIDI)

    private func updateSlotEnabled(slot: UInt8, enabled: Bool) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].enabled == enabled { return }

        p.slots[Int(slot)].enabled = enabled
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotType(slot: UInt8, typeId: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].typeId == typeId { return }

        p.slots[Int(slot)].typeId = typeId
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotParam(slot: UInt8, paramId: UInt8, value: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].params[paramId] == value { return }

        p.slots[Int(slot)].params[paramId] = value
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotRouting(slot: UInt8, inputL: UInt8, inputR: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].inputL == inputL && p.slots[Int(slot)].inputR == inputR { return }

        p.slots[Int(slot)].inputL = inputL
        p.slots[Int(slot)].inputR = inputR
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotSumToMono(slot: UInt8, sumToMono: Bool) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].sumToMono == sumToMono { return }

        p.slots[Int(slot)].sumToMono = sumToMono
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotMix(slot: UInt8, dry: UInt8, wet: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].dry == dry && p.slots[Int(slot)].wet == wet { return }

        p.slots[Int(slot)].dry = dry
        p.slots[Int(slot)].wet = wet
        patch = p
        onPatchUpdate?(p)
    }

    private func updateSlotChannelPolicy(slot: UInt8, channelPolicy: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].channelPolicy == channelPolicy { return }

        p.slots[Int(slot)].channelPolicy = channelPolicy
        patch = p
        onPatchUpdate?(p)
    }

    // MARK: - Commands (outgoing MIDI)

    func setSlotEnabled(slot: UInt8, enabled: Bool) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].enabled == enabled { return }

        p.slots[Int(slot)].enabled = enabled
        patch = p

        sendSysex(MidiProtocol.encodeSetEnabled(slot: slot, enabled: enabled))
        onPatchUpdate?(p)
    }

    func setSlotType(slot: UInt8, typeId: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].typeId == typeId { return }

        p.slots[Int(slot)].typeId = typeId
        patch = p

        sendSysex(MidiProtocol.encodeSetType(slot: slot, typeId: typeId))
        onPatchUpdate?(p)
    }

    func setSlotParam(slot: UInt8, paramId: UInt8, value: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].params[paramId] == value { return }

        p.slots[Int(slot)].params[paramId] = value
        patch = p

        sendSysex(MidiProtocol.encodeSetParam(slot: slot, paramId: paramId, value: value))
        onPatchUpdate?(p)
    }

    func setSlotRouting(slot: UInt8, inputL: UInt8, inputR: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].inputL == inputL && p.slots[Int(slot)].inputR == inputR { return }

        p.slots[Int(slot)].inputL = inputL
        p.slots[Int(slot)].inputR = inputR
        patch = p

        sendSysex(MidiProtocol.encodeSetRouting(slot: slot, inputL: inputL, inputR: inputR))
        onPatchUpdate?(p)
    }

    func setSlotSumToMono(slot: UInt8, sumToMono: Bool) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].sumToMono == sumToMono { return }

        p.slots[Int(slot)].sumToMono = sumToMono
        patch = p

        sendSysex(MidiProtocol.encodeSetSumToMono(slot: slot, sumToMono: sumToMono))
        onPatchUpdate?(p)
    }

    func setSlotMix(slot: UInt8, dry: UInt8, wet: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].dry == dry && p.slots[Int(slot)].wet == wet { return }

        p.slots[Int(slot)].dry = dry
        p.slots[Int(slot)].wet = wet
        patch = p

        sendSysex(MidiProtocol.encodeSetMix(slot: slot, dry: dry, wet: wet))
        onPatchUpdate?(p)
    }

    func setSlotChannelPolicy(slot: UInt8, channelPolicy: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }
        if p.slots[Int(slot)].channelPolicy == channelPolicy { return }

        p.slots[Int(slot)].channelPolicy = channelPolicy
        patch = p

        sendSysex(MidiProtocol.encodeSetChannelPolicy(slot: slot, channelPolicy: channelPolicy))
        onPatchUpdate?(p)
    }

    func setInputGain(gainDb: Float) {
        guard var p = patch else { return }
        // Clamp to valid range: 0 to +24 dB
        let clampedGain = max(0.0, min(24.0, gainDb))
        if abs(p.inputGainDb - clampedGain) < 0.001 { return }

        p.inputGainDb = clampedGain
        patch = p

        sendSysex(MidiProtocol.encodeSetInputGain(gainDb: clampedGain))
        onPatchUpdate?(p)
    }

    func setOutputGain(gainDb: Float) {
        guard var p = patch else { return }
        // Clamp to valid range: -12 to +12 dB
        let clampedGain = max(-12.0, min(12.0, gainDb))
        if abs(p.outputGainDb - clampedGain) < 0.001 { return }

        p.outputGainDb = clampedGain
        patch = p

        sendSysex(MidiProtocol.encodeSetOutputGain(gainDb: clampedGain))
        onPatchUpdate?(p)
    }

    func requestPatch() {
        sendSysex(MidiProtocol.encodeRequestPatch())
    }

    func requestEffectMeta() {
        sendSysex(MidiProtocol.encodeRequestMeta())
    }

    // MARK: - Send MIDI

    private func sendSysex(_ data: [UInt8]) {
        let hexString = data.map { String(format: "%02X", $0) }.joined(separator: " ")
        NSLog("[DaisyMidi] sendSysex called with \(data.count) bytes: \(hexString)")

        guard destination != 0 else {
            NSLog("[DaisyMidi] ERROR: No destination set, cannot send MIDI")
            return
        }

        guard outPort != 0 else {
            NSLog("[DaisyMidi] ERROR: No output port, cannot send MIDI")
            return
        }

        NSLog("[DaisyMidi] Sending to destination: \(destination), outPort: \(outPort)")

        let bufferSize = 1024
        var buffer = [UInt8](repeating: 0, count: bufferSize)

        buffer.withUnsafeMutableBufferPointer { ptr in
            let plist = UnsafeMutablePointer<MIDIPacketList>(OpaquePointer(ptr.baseAddress!))
            var pkt = MIDIPacketListInit(plist)

            data.withUnsafeBufferPointer { dataPtr in
                if let base = dataPtr.baseAddress {
                    pkt = MIDIPacketListAdd(plist, bufferSize, pkt, 0, data.count, base)
                }
                let status = MIDISend(outPort, destination, plist)
                if status != noErr {
                    NSLog("[DaisyMidi] MIDISend failed with status: \(status)")
                } else {
                    NSLog("[DaisyMidi] MIDISend succeeded")
                }
            }
        }
    }
}
