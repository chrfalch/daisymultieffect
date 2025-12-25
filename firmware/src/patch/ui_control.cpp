#include "patch/ui_control.h"

#include "audio/audio_engine.h"
#include "midi/midi_control.h"
#include "audio/tempo_control.h"

#include "daisy_seed.h"
#include "hid/switch.h"

namespace
{
    constexpr uint8_t kButtonCount = 2;
    constexpr uint8_t kButtonPins[kButtonCount] = {27, 28};

    static daisy::Switch g_switches[kButtonCount];
} // namespace

void UiControl::Init(daisy::DaisySeed &hw, AudioEngine &engine, MidiControl &midi, TempoControl &tempoControl)
{
    hw_ = &hw;
    engine_ = &engine;
    midi_ = &midi;
    tempoControl_ = &tempoControl;

    switches_ = g_switches;

    for (uint8_t i = 0; i < kButtonCount; i++)
        switches_[i].Init(daisy::DaisySeed::GetPin(kButtonPins[i]));
}

void UiControl::SetPatch(const PatchWireDesc &patch)
{
    currentPatch_ = patch;

    for (int i = 0; i < 2; i++)
        bindings_[i] = {patch.buttons[i].slotIndex, patch.buttons[i].mode};

    if (midi_)
        midi_->SetCurrentPatch(currentPatch_);

    if (engine_)
        engine_->ApplyPatch(currentPatch_);
}

void UiControl::HandleButtonPress(int buttonIndex)
{
    if (!engine_ || !midi_ || !tempoControl_)
        return;

    const ButtonBinding binding = bindings_[buttonIndex];

    if (binding.mode == ButtonMode::ToggleBypass)
    {
        const uint8_t slot = binding.slotIndex;
        if (slot < 12)
        {
            auto &s = engine_->Board().slots[slot];
            s.enabled = !s.enabled;

            if (slot < currentPatch_.numSlots)
                currentPatch_.slots[slot].enabled = s.enabled ? 1 : 0;

            midi_->SetCurrentPatch(currentPatch_);
            midi_->SendButtonStateChange((uint8_t)buttonIndex, slot, s.enabled);
        }
    }
    else if (binding.mode == ButtonMode::TapTempo)
    {
        tempoControl_->Tap(daisy::System::GetUs());
    }
}

void UiControl::Process()
{
    if (!switches_)
        return;

    for (uint8_t i = 0; i < kButtonCount; i++)
    {
        switches_[i].Debounce();
        if (switches_[i].RisingEdge())
            HandleButtonPress((int)i);
    }
}
