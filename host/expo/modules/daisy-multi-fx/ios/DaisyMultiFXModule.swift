import ExpoModulesCore

public class DaisyMultiFXModule: Module {
    private let controller = DaisyMidiController.shared

    public func definition() -> ModuleDefinition {
        Name("DaisyMultiFX")

        Events("onPatchUpdate", "onEffectMetaUpdate", "onConnectionStatusChange", "onStatusUpdate")

        OnCreate {
            // Wire up callbacks to emit events
            self.controller.onPatchUpdate = { [weak self] patch in
                self?.sendEvent(
                    "onPatchUpdate",
                    [
                        "patch": patch.toDictionary()
                    ])
            }

            self.controller.onEffectMetaUpdate = { [weak self] effects in
                self?.sendEvent(
                    "onEffectMetaUpdate",
                    [
                        "effects": effects.map { $0.toDictionary() }
                    ])
            }

            self.controller.onConnectionStatusChange = { [weak self] status in
                self?.sendEvent(
                    "onConnectionStatusChange",
                    [
                        "status": status.toDictionary()
                    ])
            }

            self.controller.onStatusUpdate = { [weak self] status in
                self?.sendEvent(
                    "onStatusUpdate",
                    [
                        "status": status.toDictionary()
                    ])
            }
        }

        AsyncFunction("initialize") { (promise: Promise) in
            DispatchQueue.main.async {
                self.controller.setup()
                promise.resolve(nil)
            }
        }

        AsyncFunction("disconnect") { (promise: Promise) in
            DispatchQueue.main.async {
                self.controller.teardown()
                promise.resolve(nil)
            }
        }

        Function("getConnectionStatus") { () -> [String: Any] in
            return self.controller.connectionStatus.toDictionary()
        }

        Function("isConnected") { () -> Bool in
            return self.controller.connectionStatus.connected
        }

        Function("requestPatch") {
            self.controller.requestPatch()
        }

        Function("requestEffectMeta") {
            self.controller.requestEffectMeta()
        }

        Function("getPatch") { () -> [String: Any]? in
            return self.controller.patch?.toDictionary()
        }

        Function("getEffectMeta") { () -> [[String: Any]] in
            return self.controller.effectsMeta.map { $0.toDictionary() }
        }

        Function("setSlotEnabled") { (slot: Int, enabled: Bool) in
            self.controller.setSlotEnabled(slot: UInt8(slot), enabled: enabled)
        }

        Function("setSlotType") { (slot: Int, typeId: Int) in
            self.controller.setSlotType(slot: UInt8(slot), typeId: UInt8(typeId))
        }

        Function("setSlotParam") { (slot: Int, paramId: Int, value: Int) in
            self.controller.setSlotParam(
                slot: UInt8(slot), paramId: UInt8(paramId), value: UInt8(value))
        }

        Function("setSlotRouting") { (slot: Int, inputL: Int, inputR: Int) in
            self.controller.setSlotRouting(
                slot: UInt8(slot), inputL: UInt8(inputL), inputR: UInt8(inputR))
        }

        Function("setSlotSumToMono") { (slot: Int, sumToMono: Bool) in
            self.controller.setSlotSumToMono(slot: UInt8(slot), sumToMono: sumToMono)
        }

        Function("setSlotMix") { (slot: Int, dry: Int, wet: Int) in
            self.controller.setSlotMix(slot: UInt8(slot), dry: UInt8(dry), wet: UInt8(wet))
        }

        Function("setSlotChannelPolicy") { (slot: Int, channelPolicy: Int) in
            self.controller.setSlotChannelPolicy(
                slot: UInt8(slot), channelPolicy: UInt8(channelPolicy))
        }
    }
}
