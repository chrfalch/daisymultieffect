import Combine
import CoreMIDI
import Foundation

// MARK: - Data Models

struct PatchSlot: Equatable {
    var slotIndex: UInt8 = 0
    var typeId: UInt8 = 0
    var enabled: Bool = false
    var inputL: UInt8 = 0
    var inputR: UInt8 = 0
    var sumToMono: Bool = false
    var dry: UInt8 = 0
    var wet: UInt8 = 127
    var params: [UInt8: UInt8] = [:]
}

struct Patch: Equatable {
    var numSlots: UInt8 = 0
    var slots: [PatchSlot] = []
}

struct EffectParamMeta: Equatable {
    var id: UInt8
    var name: String
}

struct EffectMeta: Equatable {
    var typeId: UInt8
    var name: String
    var params: [EffectParamMeta]
}

// MARK: - MIDI Protocol (matching C++ MidiProtocol)

enum MidiProtocol {
    static let manufacturerId: UInt8 = 0x7D

    // Sender IDs - each client type has a unique ID
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

    // Encoders (all include sender ID)
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

// MARK: - PatchState (Single Source of Truth)

/// PatchState - The single source of truth for all patch data.
///
/// All changes go through this class, which:
/// 1. Deduplicates (ignores if value unchanged)
/// 2. Updates internal state
/// 3. Publishes changes to UI via @Published
/// 4. Optionally broadcasts via MIDI (if sendMidi closure is set)
@MainActor
class PatchState: ObservableObject {
    @Published private(set) var patch: Patch?
    @Published private(set) var effectsById: [UInt8: EffectMeta] = [:]
    @Published private(set) var status: String = "Initializing..."

    /// Closure to send MIDI - set by MidiTransport
    var sendMidi: (([UInt8]) -> Void)?

    // MARK: - Commands (mutators)

    func setSlotEnabled(slot: UInt8, enabled: Bool) {
        guard var p = patch, Int(slot) < p.slots.count else { return }

        // Deduplicate
        if p.slots[Int(slot)].enabled == enabled { return }

        // Update state
        p.slots[Int(slot)].enabled = enabled
        patch = p

        // Broadcast
        sendMidi?(MidiProtocol.encodeSetEnabled(slot: slot, enabled: enabled))
    }

    func setSlotType(slot: UInt8, typeId: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }

        // Deduplicate
        if p.slots[Int(slot)].typeId == typeId { return }

        // Update state
        p.slots[Int(slot)].typeId = typeId
        patch = p

        // Broadcast
        sendMidi?(MidiProtocol.encodeSetType(slot: slot, typeId: typeId))
    }

    func setSlotParam(slot: UInt8, paramId: UInt8, value: UInt8) {
        guard var p = patch, Int(slot) < p.slots.count else { return }

        // Deduplicate
        if p.slots[Int(slot)].params[paramId] == value { return }

        // Update state
        p.slots[Int(slot)].params[paramId] = value
        patch = p

        // Broadcast
        sendMidi?(MidiProtocol.encodeSetParam(slot: slot, paramId: paramId, value: value))
    }

    func loadPatch(_ newPatch: Patch) {
        patch = newPatch
    }

    func loadEffectMeta(_ effects: [EffectMeta]) {
        var byId: [UInt8: EffectMeta] = [:]
        for fx in effects {
            byId[fx.typeId] = fx
        }
        effectsById = byId
    }

    func updateStatus(_ newStatus: String) {
        status = newStatus
    }
}

// MARK: - MidiTransport (CoreMIDI wrapper)

/// MidiTransport - Handles CoreMIDI send/receive.
///
/// This is a pure transport layer:
/// - Receives raw MIDI, decodes, routes to PatchState
/// - Sends raw MIDI when PatchState.sendMidi is called
///
/// No state management here - just bytes in/out.
// Thread-safe SysEx accumulator for multi-packet UMP messages
private final class SysExAccumulator: @unchecked Sendable {
    private var buffer: [UInt8] = []
    private var isAccumulating = false
    private let lock = NSLock()

    func process(status: UInt8, payload: [UInt8]) -> [UInt8]? {
        lock.lock()
        defer { lock.unlock() }

        switch status {
        case 0x00:  // Complete SysEx in one packet
            return payload

        case 0x01:  // Start of multi-packet SysEx
            buffer = payload
            isAccumulating = true
            return nil

        case 0x02:  // Continue
            if isAccumulating {
                buffer.append(contentsOf: payload)
            }
            return nil

        case 0x03:  // End
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

@MainActor
class MidiTransport: ObservableObject {
    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var outPort = MIDIPortRef()
    private var destination: MIDIEndpointRef = 0

    private weak var patchState: PatchState?
    private var isSetup = false

    // Thread-safe SysEx accumulator
    private let sysexAccumulator = SysExAccumulator()

    // Rate limiting
    private var lastRequestTime: Date = .distantPast
    private let minRequestInterval: TimeInterval = 0.5  // 500ms between requests

    // Callbacks for non-patch events
    var onTempoUpdate: ((Float) -> Void)?
    var onButtonStateChange: ((UInt8, UInt8, Bool) -> Void)?

    init() {}

    /// Returns true if this is the first setup call
    func setup(patchState: PatchState) -> Bool {
        guard !isSetup else {
            print("[Swift] setup() called again - ignoring")
            return false
        }
        isSetup = true

        self.patchState = patchState

        // Wire up sendMidi
        patchState.sendMidi = { [weak self] data in
            self?.sendSysex(data)
        }

        setupCoreMidi()
        return true
    }

    private func setupCoreMidi() {
        let status = MIDIClientCreate("DaisyMultiFX" as CFString, nil, nil, &client)
        guard status == noErr else {
            Task { @MainActor in
                patchState?.updateStatus("MIDI client creation failed: \(status)")
            }
            return
        }

        // Create input port with block-based callback
        var inputStatus: OSStatus = noErr
        inputStatus = MIDIInputPortCreateWithProtocol(
            client,
            "Input" as CFString,
            ._1_0,
            &inPort
        ) { [weak self] eventList, _ in
            self?.handleMidiEventList(eventList)
        }

        guard inputStatus == noErr else {
            Task { @MainActor in
                patchState?.updateStatus("MIDI input port failed: \(inputStatus)")
            }
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

        // Find destination (IAC Driver or virtual port)
        let destCount = MIDIGetNumberOfDestinations()
        var destNames: [String] = []
        for i in 0..<destCount {
            let dest = MIDIGetDestination(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            if let n = name?.takeRetainedValue() as String? {
                destNames.append(n)
                // Match IAC Driver (may appear as "IAC Driver Bus 1" or just "Bus 1")
                if n.contains("IAC") || n.contains("Bus") || n.contains("Daisy") {
                    destination = dest
                    print("[Swift] Selected destination: \(n)")
                    break
                }
            }
        }

        if destination == 0 && destCount > 0 {
            destination = MIDIGetDestination(0)
            print("[Swift] Fallback to first destination")
        }

        print("[Swift] Available destinations: \(destNames)")

        Task { @MainActor in
            let destName = destNames.first { $0.contains("IAC") || $0.contains("Bus") } ?? "unknown"
            patchState?.updateStatus("MIDI ready - \(sourceCount) sources, dest: \(destName)")
        }
    }

    private func handleMidiEventList(_ eventList: UnsafePointer<MIDIEventList>) {
        // Safety check
        guard eventList.pointee.numPackets > 0 else { return }

        print("[Swift] handleMidiEventList: \(eventList.pointee.numPackets) packets")

        var packet: UnsafePointer<MIDIEventPacket> = UnsafePointer(
            UnsafeRawPointer(eventList).advanced(
                by: MemoryLayout<MIDIEventList>.offset(of: \MIDIEventList.packet)!
            )
            .assumingMemoryBound(to: MIDIEventPacket.self)
        )

        for pktIdx in 0..<Int(eventList.pointee.numPackets) {
            let p = packet.pointee
            let wordCount = Int(p.wordCount)

            print("[Swift] Packet \(pktIdx): wordCount=\(wordCount)")

            if wordCount > 0 {
                // Extract bytes from UMP words - handle any size
                var bytes: [UInt8] = []
                withUnsafeBytes(of: p.words) { rawBuffer in
                    let umpBytes = rawBuffer.bindMemory(to: UInt8.self)
                    // For large packets, we need to read all the bytes
                    let byteCount = min(wordCount * 4, rawBuffer.count)
                    bytes = Array(umpBytes.prefix(byteCount))
                }

                // Log raw bytes
                let hexStr = bytes.prefix(20).map { String(format: "%02X", $0) }.joined(
                    separator: " ")
                print("[Swift] Raw bytes (first 20): \(hexStr)")

                // Extract SysEx from Universal MIDI Packet
                if let sysex = extractSysex(from: bytes) {
                    let capturedSysex = sysex
                    print("[Swift] Extracted SysEx, size=\(sysex.count)")
                    Task { @MainActor in
                        self.handleSysex(capturedSysex)
                    }
                } else {
                    print("[Swift] extractSysex returned nil")
                }
            }

            packet = UnsafePointer(MIDIEventPacketNext(packet))
        }
    }

    private func extractSysex(from umpData: [UInt8]) -> [UInt8]? {
        guard umpData.count >= 4 else { return nil }

        // UMP data is stored as 32-bit words in little-endian on macOS
        // Each 64-bit UMP SysEx packet contains:
        // Word 0: [data1, data0, numBytes/status, msgType/group]
        // Word 1: [data5, data4, data3, data2]

        // Read first word to check message type
        // In memory: byte0=LSB, byte3=MSB
        // So byte3 contains message type in high nibble
        let msgTypeByte = umpData[3]
        let messageType = (msgTypeByte >> 4) & 0x0F

        print(
            "[Swift] extractSysex: msgTypeByte=0x\(String(format: "%02X", msgTypeByte)), messageType=\(messageType)"
        )

        // Message type 3 = Data Messages (SysEx)
        if messageType == 0x03 {
            // This is UMP SysEx - accumulate from 64-bit packets
            var payload: [UInt8] = []
            var offset = 0
            var isComplete = false

            while offset + 8 <= umpData.count && !isComplete {
                // Read 64-bit packet (2 x 32-bit words)
                // Word layout (little-endian): [b0, b1, b2, b3] [b4, b5, b6, b7]
                // Where b3 = msgType/group, b2 = status/numBytes, b1 = data0, b0 = data1
                // And b7-b4 = data2-data5

                let statusByte = umpData[offset + 2]
                let status = (statusByte >> 4) & 0x0F  // 0=complete, 1=start, 2=continue, 3=end
                let numBytes = Int(statusByte & 0x0F)

                print("[Swift] UMP packet at \(offset): status=\(status), numBytes=\(numBytes)")

                // Extract data bytes from this 64-bit packet
                // Data is in: offset+1, offset+0, offset+7, offset+6, offset+5, offset+4
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

                // Check if this is the last packet
                if status == 0x00 || status == 0x03 {  // Complete or End
                    isComplete = true
                }

                offset += 8  // Move to next 64-bit packet
            }

            if !payload.isEmpty {
                print(
                    "[Swift] Extracted UMP SysEx payload: \(payload.prefix(20).map { String(format: "%02X", $0) }.joined(separator: " "))"
                )
                return payload
            }
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
        // Format: 7D <sender> <cmd> <data...>
        guard data.count >= 3, data[0] == MidiProtocol.manufacturerId else { return }

        let sender = data[1]
        let cmd = data[2]

        // Ignore our own messages (prevents loopback via IAC)
        if sender == MidiProtocol.Sender.swift { return }

        print(
            "[Swift] Received SysEx from sender=0x\(String(format: "%02X", sender)), cmd=0x\(String(format: "%02X", cmd))"
        )

        switch cmd {
        case MidiProtocol.Resp.patchDump:
            print("[Swift] Decoding PATCH_DUMP...")
            if let patch = decodePatchDump(data) {
                print("[Swift] Decoded patch with \(patch.slots.count) slots")
                patchState?.loadPatch(patch)
            } else {
                print("[Swift] Failed to decode patch dump")
            }

        case MidiProtocol.Resp.effectMeta:
            print("[Swift] Decoding EFFECT_META...")
            let effects = decodeEffectMeta(data)
            print("[Swift] Decoded \(effects.count) effects")
            patchState?.loadEffectMeta(effects)

        case MidiProtocol.Cmd.setEnabled:
            // Incoming command - route to PatchState (offset by 1 due to sender byte)
            if data.count >= 5 {
                let slot = data[3]
                let enabled = data[4] != 0
                patchState?.setSlotEnabled(slot: slot, enabled: enabled)
            }

        case MidiProtocol.Cmd.setType:
            if data.count >= 5 {
                let slot = data[3]
                let typeId = data[4]
                patchState?.setSlotType(slot: slot, typeId: typeId)
            }

        case MidiProtocol.Cmd.setParam:
            if data.count >= 6 {
                let slot = data[3]
                let paramId = data[4]
                let value = data[5]
                patchState?.setSlotParam(slot: slot, paramId: paramId, value: value)
            }

        case 0x40:  // Tempo broadcast
            if data.count >= 5 {
                let bpmHigh = UInt16(data[3])
                let bpmLow = UInt16(data[4])
                let bpm10x = (bpmHigh << 7) | bpmLow
                let bpm = Float(bpm10x) / 10.0
                onTempoUpdate?(bpm)
            }

        case 0x41:  // Button state
            if data.count >= 6 {
                onButtonStateChange?(data[3], data[4], data[5] != 0)
            }

        default:
            break
        }
    }

    private func decodePatchDump(_ data: [UInt8]) -> Patch? {
        // Format: 7D <sender> 13 <numSlots> [slot data...]
        guard data.count > 4 else { return nil }

        var patch = Patch()
        patch.numSlots = data[3]  // offset by 1 for sender byte

        var offset = 4  // start after numSlots
        for _ in 0..<12 {
            guard offset + 26 <= data.count else { break }

            var slot = PatchSlot()
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

            patch.slots.append(slot)
        }

        return patch
    }

    private func decodeEffectMeta(_ data: [UInt8]) -> [EffectMeta] {
        // Format: 7D <sender> 33 <numEffects> [effect data...]
        guard data.count > 4 else { return [] }

        var effects: [EffectMeta] = []
        let numEffects = Int(data[3])  // offset by 1 for sender byte
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

    func sendSysex(_ data: [UInt8]) {
        guard destination != 0 else {
            print("[Swift] sendSysex: No destination!")
            return
        }

        // Command is at data[3] (after F0, manufacturer, sender)
        let cmdByte = data.count > 3 ? String(format: "0x%02X", data[3]) : "?"
        print("[Swift] Sending SysEx, size=\(data.count), cmd=\(cmdByte)")

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

    func requestPatch() {
        sendSysex(MidiProtocol.encodeRequestPatch())
    }

    func requestEffectMeta() {
        sendSysex(MidiProtocol.encodeRequestMeta())
    }
}
