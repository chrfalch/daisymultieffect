#include "patch_protocol.h"

#include "effects_distortion.h"
#include "effects_stereo_sweep_delay.h"
#include "effects_stereo_mixer.h"

// ---- Example patch: mono → 2x dist → 2x sweep delay → stereo mix → reverb ----
PatchWireDesc MakeExamplePatch()
{
    PatchWireDesc pw{};
    pw.numSlots = 6;

    // Slot 0: (pass-through) mono input
    pw.slots[0] = {0, 0, 1, ROUTE_INPUT, ROUTE_INPUT, 1, 127, 0, (uint8_t)ChannelPolicy::ForceMono, 0, {}};

    // Slot 1: Dist A from slot0
    pw.slots[1] = {1, DistortionEffect::TypeId, 1, 0, 0, 1, 0, 127, (uint8_t)ChannelPolicy::ForceMono, 2, {{0, 90}, {1, 64}}};

    // Slot 2: SweepDelay A from dist A (mono in → stereo out)
    pw.slots[2] = {2, StereoSweepDelayEffect::TypeId, 1, 1, 1, 1, 0, 127, (uint8_t)ChannelPolicy::ForceStereo, 7, {{0, 40}, {1, 80}, {2, 127}, {3, 80}, {4, 80}, {5, 127}, {6, 60}}};

    // Slot 3: Dist B from slot0
    pw.slots[3] = {3, DistortionEffect::TypeId, 1, 0, 0, 1, 0, 127, (uint8_t)ChannelPolicy::ForceMono, 2, {{0, 70}, {1, 90}}};

    // Slot 4: SweepDelay B from dist B
    pw.slots[4] = {4, StereoSweepDelayEffect::TypeId, 1, 3, 3, 1, 0, 127, (uint8_t)ChannelPolicy::ForceStereo, 7, {{0, 30}, {1, 80}, {2, 127}, {3, 75}, {4, 85}, {5, 127}, {6, 80}}};

    // Slot 5: Stereo mixer: L=slot2, R=slot4
    pw.slots[5] = {5, StereoMixerEffect::TypeId, 1, 2, 4, 0, 0, 127, (uint8_t)ChannelPolicy::ForceStereo, 3, {{0, 64}, {1, 64}, {2, 10}}};

    pw.buttons[0] = {1, ButtonMode::ToggleBypass}; // button0 toggles dist A
    pw.buttons[1] = {2, ButtonMode::TapTempo};     // button1 tap tempo

    return pw;
}
