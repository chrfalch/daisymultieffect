// Local patch_protocol.h defines PatchWireDesc at global scope
#include "patch/patch_protocol.h"

// Core shared default patch provides the default configuration
#include "patch/default_patch.h"

// For effect type IDs
#include "protocol/sysex_protocol_c.h"

// ---- Default patch: Guitar signal chain (shared with VST) ----
PatchWireDesc MakeExamplePatch()
{
    // The core and firmware PatchWireDesc are layout-compatible
    // Copy the default patch to the firmware's type
    auto corePatch = daisyfx::MakeDefaultPatch();
    PatchWireDesc patch;
    static_assert(sizeof(patch) == sizeof(corePatch), "PatchWireDesc size mismatch");
    __builtin_memcpy(&patch, &corePatch, sizeof(patch));
    return patch;
}

// Debug patch: direct stereo passthrough (no effects).
PatchWireDesc MakePassthroughPatch()
{
    PatchWireDesc pw{};
    pw.numSlots = 0;
    pw.buttons[0] = {0, ButtonMode::Unused};
    pw.buttons[1] = {0, ButtonMode::Unused};
    return pw;
}

// Debug patch: Neural Amp only for testing amp sim
PatchWireDesc MakeNeuralAmpTestPatch()
{
    PatchWireDesc pw{};
    pw.numSlots = 1;

    // Slot 0: Neural Amp
    pw.slots[0].slotIndex = 0;
    pw.slots[0].typeId = SYSEX_EFFECT_NEURAL_AMP;
    pw.slots[0].enabled = 1;
    pw.slots[0].inputL = ROUTE_INPUT;
    pw.slots[0].inputR = ROUTE_INPUT;
    pw.slots[0].sumToMono = 1;
    pw.slots[0].wet = 127;
    pw.slots[0].dry = 0;
    pw.slots[0].channelPolicy = static_cast<uint8_t>(ChannelPolicy::Auto);
    pw.slots[0].numParams = 6;
    // Params: Model=0, InputGain=0.5, OutputGain=0.5, Bass=0.5, Mid=0.5, Treble=0.5
    pw.slots[0].params[0] = {0, 0};  // Model index 0
    pw.slots[0].params[1] = {1, 64}; // Input gain center
    pw.slots[0].params[2] = {2, 64}; // Output gain center
    pw.slots[0].params[3] = {3, 64}; // Bass center
    pw.slots[0].params[4] = {4, 64}; // Mid center
    pw.slots[0].params[5] = {5, 64}; // Treble center

    pw.buttons[0] = {0, ButtonMode::ToggleBypass};
    pw.buttons[1] = {0, ButtonMode::TapTempo};

    return pw;
}
