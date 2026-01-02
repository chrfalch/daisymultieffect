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
        destinationName: "",
        status: "Not initialized"
    )

    // Callbacks (set by the module)
    var onPatchUpdate: ((Patch) -> Void)?
    var onEffectMetaUpdate: (([EffectMeta]) -> Void)?
    var onConnectionStatusChange: ((ConnectionStatus) -> Void)?

    private init() {}

    // MARK: - Setup / Teardown

    func setup() {
        lock.lock()
        defer { lock.unlock() }

        guard !isSetup else {
            print("[DaisyMidi] Already setup")
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
                destinationName: "",
                status: "Disconnected"
            ))
    }

    // MARK: - CoreMIDI Setup

    private func setupCoreMidi() {
        let status = MIDIClientCreate("DaisyMultiFX" as CFString, nil, nil, &client)
        guard status == noErr else {
            updateConnectionStatus(
                ConnectionStatus(
                    connected: false,
                    sourceCount: 0,
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
                    destinationName: "",
                    status: "MIDI input port failed: \(inputStatus)"
                ))
            return
        }

        // Create output port
        MIDIOutputPortCreate(client, "Output" as CFString, &outPort)

        // Connect to all sources
        let sourceCount = MIDIGetNumberOfSources()
        for i in 0..<sourceCount {
            let src = MIDIGetSource(i)
            MIDIPortConnectSource(inPort, src, nil)
        }

        // Find destination
        let destCount = MIDIGetNumberOfDestinations()
        var destNames: [String] = []
        var selectedDestName = ""

        for i in 0..<destCount {
            let dest = MIDIGetDestination(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            if let n = name?.takeRetainedValue() as String? {
                destNames.append(n)
                if n.contains("IAC") || n.contains("Bus") || n.contains("Daisy") {
                    destination = dest
                    selectedDestName = n
                    print("[DaisyMidi] Selected destination: \(n)")
                    break
                }
            }
        }

        if destination == 0 && destCount > 0 {
            destination = MIDIGetDestination(0)
            selectedDestName = destNames.first ?? "unknown"
            print("[DaisyMidi] Fallback to first destination: \(selectedDestName)")
        }

        updateConnectionStatus(
            ConnectionStatus(
                connected: destination != 0,
                sourceCount: Int(sourceCount),
                destinationName: selectedDestName,
                status: "MIDI ready - \(sourceCount) sources"
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

        if messageType == 0x03 {
            var payload: [UInt8] = []
            var offset = 0
            var isComplete = false

            while offset + 8 <= umpData.count && !isComplete {
                let statusByte = umpData[offset + 2]
                let status = (statusByte >> 4) & 0x0F
                let numBytes = Int(statusByte & 0x0F)

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

                payload.append(contentsOf: packetData)

                if status == 0x03 {
                    isComplete = true
                } else if status == 0x00 {
                    if numBytes > 0 {
                        isComplete = true
                    } else {
                        break
                    }
                }

                offset += 8
            }

            if !payload.isEmpty {
                return payload
            }
        }

        // Legacy format
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

        print("[DaisyMidi] Received SysEx cmd=0x\(String(format: "%02X", cmd))")

        switch cmd {
        case MidiProtocol.Resp.patchDump:
            if let newPatch = decodePatchDump(data) {
                patch = newPatch
                onPatchUpdate?(newPatch)
            }

        case MidiProtocol.Resp.effectMeta:
            let effects = decodeEffectMeta(data)
            effectsMeta = effects
            onEffectMetaUpdate?(effects)

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
            slot.inputL = data[offset]
            offset += 1
            slot.inputR = data[offset]
            offset += 1
            slot.sumToMono = data[offset] != 0
            offset += 1
            slot.dry = data[offset]
            offset += 1
            slot.wet = data[offset]
            offset += 1
            offset += 1  // policy
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
                params.append(EffectParamMeta(id: paramId, name: paramName))
            }

            effects.append(EffectMeta(typeId: typeId, name: name, params: params))
        }

        return effects
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

    func requestPatch() {
        sendSysex(MidiProtocol.encodeRequestPatch())
    }

    func requestEffectMeta() {
        sendSysex(MidiProtocol.encodeRequestMeta())
    }

    // MARK: - Send MIDI

    private func sendSysex(_ data: [UInt8]) {
        guard destination != 0 else { return }

        let bufferSize = 1024
        var buffer = [UInt8](repeating: 0, count: bufferSize)

        buffer.withUnsafeMutableBufferPointer { ptr in
            let plist = UnsafeMutablePointer<MIDIPacketList>(OpaquePointer(ptr.baseAddress!))
            var pkt = MIDIPacketListInit(plist)

            data.withUnsafeBufferPointer { dataPtr in
                if let base = dataPtr.baseAddress {
                    pkt = MIDIPacketListAdd(plist, bufferSize, pkt, 0, data.count, base)
                }
                MIDISend(outPort, destination, plist)
            }
        }
    }
}
