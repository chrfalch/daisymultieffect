import SwiftUI

struct ContentView: View {
    @StateObject var midi = MidiController()
    @State private var lastTempo: Float?
    @State private var lastButton: String = "—"
    @State private var effects: [String] = []

    @State private var demoSlot: UInt8 = 1
    @State private var demoParam: UInt8 = 0
    @State private var demoValue: Double = 90

    var body: some View {
        VStack(spacing: 16) {
            Text("DaisyMultiFX Demo").font(.title2)

            Text(midi.status)
                .frame(maxWidth: .infinity, alignment: .leading)
                .font(.footnote)

            VStack(alignment: .leading, spacing: 8) {
                Text("Tempo: \(lastTempo.map { String(format: "%.1f BPM", $0) } ?? "—")")
                    .monospacedDigit()
                Text("Button event: \(lastButton)")
                if !effects.isEmpty {
                    Text("Effects: \(effects.joined(separator: ", "))")
                        .font(.footnote)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            VStack(alignment: .leading, spacing: 8) {
                Text("Demo param: slot \(demoSlot) id \(demoParam) value \(Int(demoValue))")
                    .font(.footnote)
                Slider(
                    value: $demoValue,
                    in: 0...127,
                    step: 1,
                    onEditingChanged: { editing in
                        if !editing {
                            midi.setParam(
                                slot: demoSlot, paramId: demoParam, value: UInt8(demoValue))
                        }
                    }
                )
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            Button("Request ALL effect meta") {
                midi.requestAllEffectMeta()
            }

            Button("Request PATCH dump") {
                midi.requestPatchDump()
            }
        }
        .padding()
        .onAppear {
            midi.onTempoUpdate = { bpm in
                DispatchQueue.main.async { lastTempo = bpm }
            }
            midi.onButtonStateChange = { btn, slot, enabled in
                DispatchQueue.main.async {
                    lastButton = "btn \(btn) slot \(slot) enabled=\(enabled)"
                }
            }
            midi.onEffectDiscovered = { _, name in
                DispatchQueue.main.async {
                    if !effects.contains(name) {
                        effects.append(name)
                    }
                }
            }
        }
    }
}
