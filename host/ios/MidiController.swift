import CoreMIDI
import Foundation

private let kManuf: UInt8 = 0x7D

final class MidiController: ObservableObject {
    var onTempoUpdate: ((Float) -> Void)?
    var onButtonStateChange: ((UInt8, UInt8, Bool) -> Void)?
    var onEffectDiscovered: ((UInt8, String) -> Void)?

    @Published var status: String = "MIDI: initializing…"

    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var outPort = MIDIPortRef()
    private var destination: MIDIEndpointRef?

    private var inSysex = false
    private var sysexBuffer: [UInt8] = []

    init() { setup() }

    private func setup() {
        MIDIClientCreateWithBlock("DaisyMultiFX" as CFString, &client) { [weak self] _ in
            // Device list changed, rebind.
            self?.selectEndpointsAndConnect()
        }
        MIDIInputPortCreateWithBlock(client, "In" as CFString, &inPort) {
            [weak self] packetList, _ in
            self?.handle(packetList: packetList.pointee)
        }
        MIDIOutputPortCreate(client, "Out" as CFString, &outPort)

        selectEndpointsAndConnect()
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

        // Pick destination.
        destination = nil
        let numDests = MIDIGetNumberOfDestinations()
        if numDests > 0 {
            var best: MIDIEndpointRef?
            for i in 0..<numDests {
                let d = MIDIGetDestination(i)
                let name = endpointName(d).lowercased()
                if name.contains("daisy") || name.contains("seed") {
                    best = d
                    break
                }
                if best == nil { best = d }
            }
            destination = best
        }

        // Pick source.
        let numSrcs = MIDIGetNumberOfSources()
        var pickedSrc: MIDIEndpointRef?
        if numSrcs > 0 {
            for i in 0..<numSrcs {
                let s = MIDIGetSource(i)
                let name = endpointName(s).lowercased()
                if name.contains("daisy") || name.contains("seed") {
                    pickedSrc = s
                    break
                }
                if pickedSrc == nil { pickedSrc = s }
            }
        }

        if let src = pickedSrc {
            MIDIPortConnectSource(inPort, src, nil)
        }

        if let dest = destination {
            status = "MIDI: connected to \(endpointName(dest))"
        } else {
            status = "MIDI: no destinations"
        }
    }

    func sendSysex(_ bytes: [UInt8]) {
        guard let dest = destination else { return }
        let data = bytes
        // Allocate an actual packet list buffer (MIDIPacketList is variable-length).
        let bufferSize = max(1024, data.count + 64)
        var packetBytes = [UInt8](repeating: 0, count: bufferSize)
        packetBytes.withUnsafeMutableBytes { raw in
            guard let plist = raw.baseAddress?.assumingMemoryBound(to: MIDIPacketList.self) else {
                return
            }
            var pkt = MIDIPacketListInit(plist)
            data.withUnsafeBytes { syx in
                guard let base = syx.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                    return
                }
                pkt = MIDIPacketListAdd(plist, bufferSize, pkt, 0, data.count, base)
            }
            if pkt != nil {
                MIDISend(outPort, dest, plist)
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

    // MARK: Parse

    private func handle(packetList: MIDIPacketList) {
        var packet = packetList.packet
        for _ in 0..<packetList.numPackets {
            let bytes = Mirror(reflecting: packet.data).children
                .prefix(Int(packet.length))
                .compactMap { $0.value as? UInt8 }
            handle(bytes: bytes)
            packet = MIDIPacketNext(&packet).pointee
        }
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

        default:
            break
        }
    }
}
