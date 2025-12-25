import SwiftUI

// Separate view for each parameter slider to isolate state
struct ParamSliderView: View {
    let slotIndex: UInt8
    let paramId: UInt8
    let paramName: String
    let currentValue: UInt8
    let midi: MidiController

    @State private var localValue: Double = 0
    @State private var isDragging = false
    @State private var lastSentValue: UInt8 = 255

    private func sendCurrentValue(force: Bool = false) {
        let sendVal = UInt8(max(0, min(127, Int(localValue))))
        if force || sendVal != lastSentValue {
            lastSentValue = sendVal
            midi.setParam(slot: slotIndex, paramId: paramId, value: sendVal)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("\(paramName): \(Int(localValue))")
                .font(.footnote)
                .monospacedDigit()
            Slider(
                value: $localValue,
                in: 0...127,
                step: 1,
                onEditingChanged: { editing in
                    isDragging = editing
                    if !editing {
                        // Ensure the final value is sent even if it matches the last streamed value.
                        sendCurrentValue(force: true)
                    }
                }
            )
            .onChange(of: localValue) { _, _ in
                // Stream while dragging for immediate feedback.
                if isDragging {
                    sendCurrentValue(force: false)
                }
            }
        }
        .onAppear {
            localValue = Double(currentValue)
            lastSentValue = currentValue
        }
        .onChange(of: currentValue) { _, newValue in
            // Keep UI synced with device, but never fight the user mid-drag.
            if !isDragging {
                localValue = Double(newValue)
                lastSentValue = newValue
            }
        }
    }
}

struct ContentView: View {
    @StateObject var midi = MidiController()
    @State private var lastTempo: Float?
    @State private var lastButton: String = "-"

    private func effectName(_ typeId: UInt8) -> String {
        midi.effectsById[typeId]?.name ?? (typeId == 0 ? "(none)" : "Type \(typeId)")
    }

    private func paramName(typeId: UInt8, paramId: UInt8) -> String {
        guard let meta = midi.effectsById[typeId] else { return "Param \(paramId)" }
        return meta.params.first(where: { $0.id == paramId })?.name ?? "Param \(paramId)"
    }

    var body: some View {
        VStack(spacing: 12) {
            Text("DaisyMultiFX Demo").font(.title2)

            Text(midi.status)
                .frame(maxWidth: .infinity, alignment: .leading)
                .font(.footnote)

            VStack(alignment: .leading, spacing: 6) {
                Text("Tempo: \(lastTempo.map { String(format: "%.1f BPM", $0) } ?? "-")")
                    .monospacedDigit()
                Text("Button event: \(lastButton)")
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            HStack {
                Button("Refresh effect meta") { midi.requestAllEffectMeta() }
                Button("Refresh patch") { midi.requestPatchDump() }
            }

            if let patch = midi.patch, patch.numSlots > 0 {
                let activeSlots = Array(patch.slots.prefix(Int(patch.numSlots)))
                List {
                    ForEach(activeSlots, id: \.slotIndex) { slot in
                        Section("Slot \(slot.slotIndex): \(effectName(slot.typeId))") {
                            Toggle(
                                "Enabled",
                                isOn: Binding(
                                    get: { slot.enabled },
                                    set: { newValue in
                                        midi.setSlotEnabled(slot: slot.slotIndex, enabled: newValue)
                                    }
                                )
                            )

                            if !midi.effectsById.isEmpty {
                                Picker(
                                    "Effect",
                                    selection: Binding(
                                        get: { slot.typeId },
                                        set: { newTypeId in
                                            midi.setSlotType(
                                                slot: slot.slotIndex, typeId: newTypeId)
                                        }
                                    )
                                ) {
                                    Text("(none)").tag(UInt8(0))
                                    ForEach(
                                        midi.effectsById.values.sorted(by: { $0.typeId < $1.typeId }
                                        ), id: \.typeId
                                    ) { fx in
                                        Text(fx.name).tag(fx.typeId)
                                    }
                                }
                            }

                            let metaParams = midi.effectsById[slot.typeId]?.params ?? []
                            let paramIds: [UInt8] = {
                                if !metaParams.isEmpty {
                                    return metaParams.map { $0.id }
                                }
                                return slot.params.keys.sorted()
                            }()

                            ForEach(paramIds, id: \.self) { pid in
                                ParamSliderView(
                                    slotIndex: slot.slotIndex,
                                    paramId: pid,
                                    paramName: paramName(typeId: slot.typeId, paramId: pid),
                                    currentValue: slot.params[pid] ?? 0,
                                    midi: midi
                                )
                                .id("\(slot.slotIndex)-\(pid)")
                            }
                        }
                    }
                }
            } else {
                Text("No patch loaded yet. Tap 'Refresh patch'.")
                    .font(.footnote)
                    .frame(maxWidth: .infinity, alignment: .leading)
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

            // Kick initial state.
            midi.requestAllEffectMeta()
            midi.requestPatchDump()
        }
    }
}
