#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "hid/usb_midi.h"

#include "patch_protocol.h"
#include "pedalboard.h"
#include "tempo.h"

// USB-MIDI (device) + SysEx-based control plane.
// Keeps main.cpp focused on audio/DSP while this module handles:
// - SysEx RX reassembly
// - Request/reply commands (effect list, patch dump)
// - Parameter updates (applied in audio callback)
class MidiControl
{
public:
    using Board = PedalBoardRuntime<12>;

    void Init(daisy::DaisySeed &hw, Board &board, TempoSource &tempo);

    // Provide the currently-active patch for PATCH_DUMP responses.
    void SetCurrentPatch(const PatchWireDesc &patch);

    // Call in main loop.
    void Process();

    // Call at start of audio callback (block-rate) to apply queued param updates.
    void ApplyPendingParamInAudioThread();

    void SendTempoUpdate(float bpm);
    void SendButtonStateChange(uint8_t btn, uint8_t slot, bool enabled);

private:
    static void OnUsbMidiRx(uint8_t *data, size_t size, void *context);

    void SendSysEx(const uint8_t *data, size_t len);

    void QueueSysexIfFree(const uint8_t *data, size_t len);
    void HandleSysexMessage(const uint8_t *bytes, size_t len);

    void SendEffectList();
    void SendPatchDump();

    static uint8_t EncodeRoute(uint8_t r);

    daisy::DaisySeed *hw_ = nullptr;
    Board *board_ = nullptr;
    TempoSource *tempo_ = nullptr;
    daisy::MidiUsbTransport usb_midi_;

    PatchWireDesc current_patch_{};

    // Param updates should be applied on the audio thread to avoid races with DSP.
    volatile bool param_pending_ = false;
    volatile uint8_t param_slot_ = 0;
    volatile uint8_t param_id_ = 0;
    volatile uint8_t param_value_ = 0; // 0..127

    // ---- USB MIDI RX: reconstruct SysEx from byte stream ----
    uint8_t sysex_build_[512]{};
    size_t sysex_build_len_ = 0;
    bool in_sysex_ = false;

    uint8_t sysex_msg_[512]{};
    size_t sysex_msg_len_ = 0;
    volatile bool sysex_ready_ = false;
};
