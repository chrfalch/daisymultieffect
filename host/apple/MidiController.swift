import Combine
import CoreMIDI
import Foundation

private let kManuf: UInt8 = 0x7D

struct EffectParamMeta: Identifiable, Hashable {
    let id: UInt8
    let kind: UInt8
    let name: String
}

struct EffectMetaV2: Identifiable, Hashable {
    var id: UInt8 { typeId }
    let typeId: UInt8
    let name: String
    let params: [EffectParamMeta]
}

struct PatchSlot: Identifiable, Hashable {
    var id: UInt8 { slotIndex }
    let slotIndex: UInt8
    var typeId: UInt8
    var enabled: Bool
    var dry: UInt8
    var wet: UInt8
    var params: [UInt8: UInt8]  // paramId -> 0..127
}

struct PatchDump: Hashable {
    var numSlots: UInt8
    var slots: [PatchSlot]  // always 12 entries as sent by firmware
}

final class MidiController: ObservableObject {
    var onTempoUpdate: ((Float) -> Void)?
    var onButtonStateChange: ((UInt8, UInt8, Bool) -> Void)?
    var onEffectDiscovered: ((UInt8, String) -> Void)?

    @Published var status: String = "MIDI: initializing…"

    @Published private(set) var effectsById: [UInt8: EffectMetaV2] = [:]
    @Published private(set) var patch: PatchDump?

    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var outPort = MIDIPortRef()
    private var destination: MIDIEndpointRef?

    private let midiQueue = DispatchQueue(label: "MidiController.midi")

    private var inSysex = false
    private var sysexBuffer: [UInt8] = []

    init() { setup() }

    private func setup() {
        MIDIClientCreateWithBlock("DaisyMultiFX" as CFString, &client) { [weak self] _ in
            // Device list changed, rebind.
            self?.midiQueue.async {
                self?.selectEndpointsAndConnect()
            }
        }
        MIDIInputPortCreateWithBlock(client, "In" as CFString, &inPort) {
            [weak self] packetList, _ in
            self?.handle(packetList: packetList.pointee)
        }
        MIDIOutputPortCreate(client, "Out" as CFString, &outPort)

        midiQueue.async {
            self.selectEndpointsAndConnect()
        }
    }

    private func endpointName(_ endpoint: MIDIEndpointRef) -> String {
        var str: Unmanaged<CFString>?
        let err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &str)
        if err == noErr, let s = str?.takeRetainedValue() {
            return s as String
        }
        return "(unknown)"
    }

    private func selectEndpointsAndConnect() {
        // Disconnect previous sources by recreating the input port connection state.
        // (CoreMIDI doesn't provide a simple 'disconnect all' on the port.)

        // Pick destination - look for Daisy, IAC Driver, network sessions, or fall back to first available
        destination = nil
        let numDests = MIDIGetNumberOfDestinations()
        print("MIDI: Scanning \(numDests) destinations...")
        if numDests > 0 {
            var daisyEndpoint: MIDIEndpointRef?
            var iacEndpoint: MIDIEndpointRef?
            var networkEndpoint: MIDIEndpointRef?
            var firstEndpoint: MIDIEndpointRef?

            for i in 0..<numDests {
                let d = MIDIGetDestination(i)
                let name = endpointName(d)
                let lname = name.lowercased()
                print("  Dest \(i): \(name)")

                // Prefer Daisy device or DaisyMultiFX VST/standalone
                if lname.contains("daisy") || lname.contains("seed") || lname.contains("multifx") {
                    daisyEndpoint = d
                }
                // IAC Driver for virtual MIDI (connecting to VST/standalone)
                else if lname.contains("iac") {
                    if iacEndpoint == nil { iacEndpoint = d }
                }
                // Remember network sessions as fallback
                else if lname.contains("network") || lname.contains("session")
                    || lname.contains("rtpmidi")
                {
                    if networkEndpoint == nil { networkEndpoint = d }
                }
                // Remember first as last resort
                else if firstEndpoint == nil {
                    firstEndpoint = d
                }
            }
            // Priority: Daisy > IAC > network > first available
            destination = daisyEndpoint ?? iacEndpoint ?? networkEndpoint ?? firstEndpoint
        }

        // Pick source - same logic (Daisy > IAC > network > first available)
        let numSrcs = MIDIGetNumberOfSources()
        print("MIDI: Scanning \(numSrcs) sources...")
        var daisySrc: MIDIEndpointRef?
        var iacSrc: MIDIEndpointRef?
        var networkSrc: MIDIEndpointRef?
        var firstSrc: MIDIEndpointRef?

        if numSrcs > 0 {
            for i in 0..<numSrcs {
                let s = MIDIGetSource(i)
                let name = endpointName(s)
                let lname = name.lowercased()
                print("  Src \(i): \(name)")

                if lname.contains("daisy") || lname.contains("seed") || lname.contains("multifx") {
                    daisySrc = s
                } else if lname.contains("iac") {
                    if iacSrc == nil { iacSrc = s }
                } else if lname.contains("network") || lname.contains("session")
                    || lname.contains("rtpmidi")
                {
                    if networkSrc == nil { networkSrc = s }
                } else if firstSrc == nil {
                    firstSrc = s
                }
            }
        }

        let src = daisySrc ?? iacSrc ?? networkSrc ?? firstSrc
        if let src = src {
            MIDIPortConnectSource(inPort, src, nil)
            print("MIDI: Connected to source: \(endpointName(src))")
        }

        if let dest = destination {
            let destName = endpointName(dest)
            print("MIDI: Selected destination: \(destName)")
            DispatchQueue.main.async {
                self.status = "MIDI: connected to \(destName)"
            }
        } else {
            print("MIDI: No destinations found!")
            print("Available destinations:")
            for i in 0..<MIDIGetNumberOfDestinations() {
                let d = MIDIGetDestination(i)
                print("  - \(endpointName(d))")
            }
            DispatchQueue.main.async {
                self.status = "MIDI: no destinations"
            }
        }
    }

    func sendSysex(_ bytes: [UInt8]) {
        midiQueue.async {
            guard let dest = self.destination else { return }
            let data = bytes
            // Allocate an actual packet list buffer (MIDIPacketList is variable-length).
            let bufferSize = max(1024, data.count + 64)
            var packetBytes = [UInt8](repeating: 0, count: bufferSize)
            packetBytes.withUnsafeMutableBytes { raw in
                guard let plist = raw.baseAddress?.assumingMemoryBound(to: MIDIPacketList.self)
                else {
                    return
                }
                var pkt = MIDIPacketListInit(plist)
                data.withUnsafeBytes { syx in
                    guard let base = syx.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                        return
                    }
                    pkt = MIDIPacketListAdd(plist, bufferSize, pkt, 0, data.count, base)
                }
                // With our buffer sizing, MIDIPacketListAdd is expected to succeed.
                MIDISend(self.outPort, dest, plist)
            }
        }
    }

    // Commands
    func requestAllEffectMeta() { sendSysex([0xF0, kManuf, 0x32, 0xF7]) }
    func requestPatchDump() { sendSysex([0xF0, kManuf, 0x12, 0xF7]) }

    func setParam(slot: UInt8, paramId: UInt8, value: UInt8) {
        // F0 7D 20 <slot> <paramId> <value> F7
        sendSysex([0xF0, kManuf, 0x20, slot & 0x7F, paramId & 0x7F, value & 0x7F, 0xF7])
    }

    func setSlotEnabled(slot: UInt8, enabled: Bool) {
        // F0 7D 21 <slot> <enabled> F7
        sendSysex([0xF0, kManuf, 0x21, slot & 0x7F, enabled ? 1 : 0, 0xF7])
    }

    func setSlotType(slot: UInt8, typeId: UInt8) {
        // F0 7D 22 <slot> <typeId> F7
        sendSysex([0xF0, kManuf, 0x22, slot & 0x7F, typeId & 0x7F, 0xF7])
    }

    // MARK: Parse

    private func handle(packetList: MIDIPacketList) {
        // MIDIPacketList is variable-length, so we need to work with it carefully
        // Access packet data directly from the first packet
        let packet = packetList.packet
        let length = Int(packet.length)

        guard length > 0 && length <= 256 else { return }

        // Extract bytes from the tuple using reflection (safe for single packet)
        let mirror = Mirror(reflecting: packet.data)
        var bytes = [UInt8]()
        bytes.reserveCapacity(length)

        for (index, child) in mirror.children.enumerated() {
            if index >= length { break }
            if let byte = child.value as? UInt8 {
                bytes.append(byte)
            }
        }

        if !bytes.isEmpty {
            handle(bytes: bytes)
        }

        // Note: For simplicity, we only handle the first packet.
        // SysEx messages from our VST should fit in a single packet.
    }

    private func unpackQ16_16(_ five: ArraySlice<UInt8>) -> Float? {
        let b = Array(five.prefix(5))
        guard b.count == 5 else { return nil }
        var u: UInt32 = 0
        u |= UInt32(b[0])
        u |= UInt32(b[1]) << 7
        u |= UInt32(b[2]) << 14
        u |= UInt32(b[3]) << 21
        u |= UInt32(b[4]) << 28
        let s = Int32(bitPattern: u)
        return Float(s) / 65536.0
    }

    private func handle(bytes: [UInt8]) {
        // Reassemble SysEx across CoreMIDI packets.
        for b in bytes {
            if !inSysex {
                if b == 0xF0 {
                    inSysex = true
                    sysexBuffer.removeAll(keepingCapacity: true)
                    sysexBuffer.append(b)
                }
                continue
            }
            sysexBuffer.append(b)
            if b == 0xF7 {
                inSysex = false
                handleSysexMessage(sysexBuffer)
                sysexBuffer.removeAll(keepingCapacity: true)
            }
        }
    }

    private func handleSysexMessage(_ bytes: [UInt8]) {
        guard bytes.count >= 4 else { return }
        guard bytes.first == 0xF0, bytes.last == 0xF7 else { return }
        guard bytes[1] == kManuf else { return }

        let type = bytes[2]
        switch type {
        case 0x40:
            // F0 7D 40 <btn> <slot> <enabled> F7
            guard bytes.count >= 7 else { return }
            let btn = bytes[3]
            let slot = bytes[4]
            let enabled = bytes[5] != 0
            onButtonStateChange?(btn, slot, enabled)

        case 0x41:
            // F0 7D 41 <bpmQ16_16_5bytes> F7
            guard bytes.count >= 3 + 5 + 1 else { return }
            if let bpm = unpackQ16_16(bytes[3..<(3 + 5)]) {
                onTempoUpdate?(bpm)
            }

        case 0x34:
            // Effect discovered: F0 7D 34 <typeId> <nameLen> <name...> F7
            guard bytes.count >= 6 else { return }
            let typeId = bytes[3]
            let nameLen = Int(bytes[4])
            let start = 5
            let end = min(start + nameLen, bytes.count - 1)
            if end > start {
                let nameBytes = bytes[start..<end]
                let name = String(bytes: nameBytes, encoding: .ascii) ?? "(invalid)"
                onEffectDiscovered?(typeId, name)
            }

        case 0x35:
            // Effect meta v2:
            // F0 7D 35 <typeId> <nameLen> <name...> <numParams>
            //   (paramId kind nameLen name...)xN
            // F7
            guard bytes.count >= 7 else { return }
            let typeId = bytes[3]
            let nameLen = Int(bytes[4])
            var idx = 5
            let nameEnd = min(idx + nameLen, bytes.count - 1)
            let name = String(bytes: bytes[idx..<nameEnd], encoding: .ascii) ?? "(invalid)"
            idx = nameEnd
            guard idx < bytes.count - 1 else { return }
            let numParams = Int(bytes[idx])
            idx += 1
            var params: [EffectParamMeta] = []
            params.reserveCapacity(numParams)
            for _ in 0..<numParams {
                guard idx + 3 <= bytes.count - 1 else { break }
                let pid = bytes[idx]
                idx += 1
                let kind = bytes[idx]
                idx += 1
                let pNameLen = Int(bytes[idx])
                idx += 1
                let pNameEnd = min(idx + pNameLen, bytes.count - 1)
                let pName = String(bytes: bytes[idx..<pNameEnd], encoding: .ascii) ?? "Param \(pid)"
                idx = pNameEnd
                params.append(EffectParamMeta(id: pid, kind: kind, name: pName))
            }

            let meta = EffectMetaV2(typeId: typeId, name: name, params: params)
            DispatchQueue.main.async {
                self.effectsById[typeId] = meta
            }

        case 0x13:
            // Patch dump:
            // F0 7D 13 <numSlots>
            //  12x slots: slotIndex typeId enabled inputL inputR sumToMono dry wet policy numParams (id val)x8
            //  2x buttons: slotIndex mode
            // F7
            guard bytes.count >= 5 else { return }
            let numSlots = bytes[3]
            var idx = 4
            var slots: [PatchSlot] = []
            slots.reserveCapacity(12)

            for _ in 0..<12 {
                guard idx + 10 <= bytes.count - 1 else { break }
                let slotIndex = bytes[idx]
                idx += 1
                let typeId = bytes[idx]
                idx += 1
                let enabled = bytes[idx] != 0
                idx += 1
                _ = bytes[idx]
                idx += 1  // inputL
                _ = bytes[idx]
                idx += 1  // inputR
                _ = bytes[idx]
                idx += 1  // sumToMono
                let dry = bytes[idx]
                idx += 1
                let wet = bytes[idx]
                idx += 1
                _ = bytes[idx]
                idx += 1  // policy
                let nParams = Int(bytes[idx])
                idx += 1
                var p: [UInt8: UInt8] = [:]
                p.reserveCapacity(min(nParams, 8))
                for k in 0..<8 {
                    guard idx + 2 <= bytes.count - 1 else { break }
                    let pid = bytes[idx]
                    idx += 1
                    let val = bytes[idx]
                    idx += 1
                    if k < nParams {
                        p[pid] = val
                    }
                }
                slots.append(
                    PatchSlot(
                        slotIndex: slotIndex, typeId: typeId, enabled: enabled, dry: dry, wet: wet,
                        params: p))
            }

            let pd = PatchDump(numSlots: numSlots, slots: slots)
            DispatchQueue.main.async {
                self.patch = pd
            }

        default:
            break
        }
    }
}
