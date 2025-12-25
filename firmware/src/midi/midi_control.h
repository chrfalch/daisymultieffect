#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "hid/usb_midi.h"
#include "util/scopedirqblocker.h"

#include "patch/patch_protocol.h"
#include "audio/pedalboard.h"
#include "audio/tempo.h"

class AudioEngine;

// USB-MIDI (device) + SysEx-based control plane.
// Keeps main.cpp focused on audio/DSP while this module handles:
// - SysEx RX reassembly
// - Request/reply commands (effect list, patch dump)
// - Parameter updates (applied in audio callback)
class MidiControl
{
public:
    using Board = PedalBoardRuntime<12>;

    enum class PendingKind : uint8_t
    {
        None = 0,
        SetParam = 1,
        SetSlotEnabled = 2,
        SetSlotType = 3,
    };

    void Init(daisy::DaisySeed &hw, AudioEngine &engine, Board &board, TempoSource &tempo);

    // Provide the currently-active patch for PATCH_DUMP responses.
    void SetCurrentPatch(const PatchWireDesc &patch);

    // Call in main loop.
    void Process();

    // Call at start of audio callback (block-rate) to apply queued updates.
    void ApplyPendingInAudioThread();

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
    AudioEngine *engine_ = nullptr;
    Board *board_ = nullptr;
    TempoSource *tempo_ = nullptr;
    daisy::MidiUsbTransport usb_midi_;

    PatchWireDesc current_patch_{};

    // Updates should be applied on the audio thread to avoid races with DSP.
    // This is written by the main loop and read/cleared by the audio callback ISR.
    // Pack fields into one aligned word to avoid torn reads across fields.
    // Layout: kind | slot<<8 | id<<16 | value<<24
    volatile uint32_t pending_word_ = 0;

    // When true, the main loop will transmit a patch dump (set by audio thread).
    volatile bool tx_patch_dump_pending_ = false;

    // Scratch storage used when re-applying patches.
    PatchWireDesc pending_patch_{};

    // ---- USB MIDI RX: reconstruct SysEx from byte stream ----
    uint8_t sysex_build_[512]{};
    size_t sysex_build_len_ = 0;
    bool in_sysex_ = false;

    uint8_t sysex_msg_[512]{};
    size_t sysex_msg_len_ = 0;
    volatile bool sysex_ready_ = false;
};
