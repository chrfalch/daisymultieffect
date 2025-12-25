#include "patch/patch_protocol.h"

#include "effects/delay.h"
#include "effects/distortion.h"
#include "effects/stereo_sweep_delay.h"
#include "effects/stereo_mixer.h"

// ---- Default patch: 12 empty slots ready for configuration via MIDI/UI ----
PatchWireDesc MakeExamplePatch()
{
    PatchWireDesc pw{};
    pw.numSlots = 12;

    // Initialize all slots as empty (typeId=0 means no effect)
    // Routing: slot 0 reads from hardware input, each subsequent slot reads from the previous one (serial chain).
    for (uint8_t i = 0; i < 12; i++)
    {
        uint8_t inputRoute = (i == 0) ? ROUTE_INPUT : (i - 1);
        pw.slots[i] = {
            i,                            // slotIndex
            0,                            // typeId (0 = no effect)
            0,                            // enabled
            inputRoute,                   // inputL (from previous slot or audio input)
            inputRoute,                   // inputR (from previous slot or audio input)
            0,                            // sumToMono
            0,                            // dry
            127,                          // wet
            (uint8_t)ChannelPolicy::Auto, // channelPolicy
            0,                            // numParams
            {}                            // params (empty)
        };
    }

    pw.buttons[0] = {0, ButtonMode::ToggleBypass};
    pw.buttons[1] = {0, ButtonMode::TapTempo};

    return pw;
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
