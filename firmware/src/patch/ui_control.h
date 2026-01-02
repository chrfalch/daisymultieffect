#pragma once

#include <cstdint>

#include "patch/patch_protocol.h"

namespace daisy
{
    class DaisySeed;
    class Switch;
} // namespace daisy

class AudioProcessor;
class MidiControl;
class TempoControl;

// Owns physical UI (buttons/switches) and translates events into actions:
// - Toggle bypass for a bound slot
// - Tap tempo
// Also keeps the "current patch" copy in sync for MIDI PATCH_DUMP replies.
class UiControl
{
public:
    void Init(daisy::DaisySeed &hw, AudioProcessor &processor, MidiControl &midi, TempoControl &tempoControl);

    // Sets the active patch. Applies it to AudioEngine and informs MidiControl.
    void SetPatch(const PatchWireDesc &patch);

    // Call from the main loop.
    void Process();

private:
    struct ButtonBinding
    {
        uint8_t slotIndex;
        ButtonMode mode;
    };

    void HandleButtonPress(int buttonIndex);

    daisy::DaisySeed *hw_ = nullptr;
    AudioProcessor *processor_ = nullptr;
    MidiControl *midi_ = nullptr;
    TempoControl *tempoControl_ = nullptr;

    daisy::Switch *switches_ = nullptr;

    ButtonBinding bindings_[2] = {{0xFF, ButtonMode::Unused}, {0xFF, ButtonMode::Unused}};
    PatchWireDesc currentPatch_{};
};
