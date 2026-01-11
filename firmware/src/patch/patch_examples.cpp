// Local patch_protocol.h defines PatchWireDesc at global scope
#include "patch/patch_protocol.h"

// Core shared default patch provides the default configuration
#include "patch/default_patch.h"

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
