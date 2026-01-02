import SwiftUI

// MARK: - Slot View

struct SlotView: View {
    let slot: PatchSlot
    let patchState: PatchState
    let effectName: String

    var body: some View {
        Section("Slot \(slot.slotIndex): \(effectName)") {
            Toggle(
                "Enabled",
                isOn: Binding(
                    get: { slot.enabled },
                    set: { patchState.setSlotEnabled(slot: slot.slotIndex, enabled: $0) }
                ))

            if !patchState.effectsById.isEmpty {
                Picker(
                    "Effect",
                    selection: Binding(
                        get: { slot.typeId },
                        set: { patchState.setSlotType(slot: slot.slotIndex, typeId: $0) }
                    )
                ) {
                    Text("(none)").tag(UInt8(0))
                    ForEach(
                        patchState.effectsById.values.sorted(by: { $0.typeId < $1.typeId }),
                        id: \.typeId
                    ) { fx in
                        Text(fx.name).tag(fx.typeId)
                    }
                }
            }

            // Parameter sliders
            let metaParams = patchState.effectsById[slot.typeId]?.params ?? []
            let paramIds: [UInt8] =
                metaParams.isEmpty
                ? slot.params.keys.sorted()
                : metaParams.map { $0.id }

            ForEach(paramIds, id: \.self) { pid in
                ParamSliderView(
                    slotIndex: slot.slotIndex,
                    paramId: pid,
                    paramName: paramName(paramId: pid),
                    currentValue: slot.params[pid] ?? 0,
                    patchState: patchState
                )
            }
        }
    }

    private func paramName(paramId: UInt8) -> String {
        guard let meta = patchState.effectsById[slot.typeId] else {
            return "Param \(paramId)"
        }
        return meta.params.first(where: { $0.id == paramId })?.name ?? "Param \(paramId)"
    }
}

// MARK: - Parameter Slider View

struct ParamSliderView: View {
    let slotIndex: UInt8
    let paramId: UInt8
    let paramName: String
    let currentValue: UInt8
    let patchState: PatchState

    @State private var localValue: Double = 0
    @State private var isDragging = false

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
                        sendValue()
                    }
                }
            )
            .onChange(of: localValue) { _, _ in
                if isDragging {
                    sendValue()
                }
            }
        }
        .onAppear {
            localValue = Double(currentValue)
        }
        .onChange(of: currentValue) { _, newValue in
            if !isDragging {
                localValue = Double(newValue)
            }
        }
    }

    private func sendValue() {
        let value = UInt8(max(0, min(127, Int(localValue))))
        patchState.setSlotParam(slot: slotIndex, paramId: paramId, value: value)
    }
}

// MARK: - Main Content View

struct ContentView: View {
    @StateObject private var patchState = PatchState()
    @StateObject private var midiTransport = MidiTransport()

    @State private var lastTempo: Float?
    @State private var lastButton: String = "-"

    var body: some View {
        VStack(spacing: 12) {
            Text("DaisyMultiFX").font(.title2)

            Text(patchState.status)
                .frame(maxWidth: .infinity, alignment: .leading)
                .font(.footnote)

            VStack(alignment: .leading, spacing: 6) {
                Text("Tempo: \(lastTempo.map { String(format: "%.1f BPM", $0) } ?? "-")")
                    .monospacedDigit()
                Text("Button event: \(lastButton)")
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            HStack {
                Button("Refresh effects") { midiTransport.requestEffectMeta() }
                Button("Refresh patch") { midiTransport.requestPatch() }
            }

            if let patch = patchState.patch, patch.numSlots > 0 {
                let activeSlots = Array(patch.slots.prefix(Int(patch.numSlots)))
                List {
                    ForEach(activeSlots, id: \.slotIndex) { slot in
                        SlotView(
                            slot: slot,
                            patchState: patchState,
                            effectName: effectName(slot.typeId)
                        )
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
            // Setup MIDI transport with PatchState - only request on first setup
            let isFirstSetup = midiTransport.setup(patchState: patchState)

            midiTransport.onTempoUpdate = { bpm in
                Task { @MainActor in lastTempo = bpm }
            }
            midiTransport.onButtonStateChange = { btn, slot, enabled in
                Task { @MainActor in
                    lastButton = "btn \(btn) slot \(slot) enabled=\(enabled)"
                }
            }

            // Request initial state only on first setup
            if isFirstSetup {
                midiTransport.requestEffectMeta()
                midiTransport.requestPatch()
            }
        }
    }

    private func effectName(_ typeId: UInt8) -> String {
        patchState.effectsById[typeId]?.name ?? (typeId == 0 ? "(none)" : "Type \(typeId)")
    }
}
