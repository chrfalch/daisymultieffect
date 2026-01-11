#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "hid/usb_midi.h"
#include "util/scopedirqblocker.h"

#include "patch/patch_protocol.h"
#include "audio/pedalboard.h"
#include "audio/tempo.h"

class AudioProcessor;

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
        SetSlotRouting = 4,
        SetSlotSumToMono = 5,
        SetSlotMix = 6,
        SetSlotChannelPolicy = 7,
    };

    void Init(daisy::DaisySeed &hw, AudioProcessor &processor, Board &board, TempoSource &tempo);

    // Provide the currently-active patch for PATCH_DUMP responses.
    void SetCurrentPatch(const PatchWireDesc &patch);

    // Call in main loop.
    void Process();

    // Call at start of audio callback (block-rate) to apply queued updates.
    void ApplyPendingInAudioThread();

    void SendTempoUpdate(float bpm);
    void SendButtonStateChange(uint8_t btn, uint8_t slot, bool enabled);

    // Send individual state changes (for bidirectional sync)
    void SendSetParam(uint8_t slot, uint8_t paramId, uint8_t value);
    void SendSetEnabled(uint8_t slot, bool enabled);
    void SendSetType(uint8_t slot, uint8_t typeId);

private:
    static void OnUsbMidiRx(uint8_t *data, size_t size, void *context);

    void SendSysEx(const uint8_t *data, size_t len);

    void QueueSysexIfFree(const uint8_t *data, size_t len);
    void HandleSysexMessage(const uint8_t *bytes, size_t len);

    void SendEffectList();
    void SendPatchDump();

    static uint8_t EncodeRoute(uint8_t r);

    daisy::DaisySeed *hw_ = nullptr;
    AudioProcessor *processor_ = nullptr;
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

    // Pending individual state changes to send (more efficient than full patch dump)
    volatile uint32_t tx_pending_param_ = 0;   // Pack: slot<<24 | paramId<<16 | value<<8 | 1
    volatile uint32_t tx_pending_enabled_ = 0; // Pack: slot<<24 | enabled<<8 | 2
    volatile uint32_t tx_pending_type_ = 0;    // Pack: slot<<24 | typeId<<8 | 3

    // Scratch storage used when re-applying patches.
    PatchWireDesc pending_patch_{};

    // ---- USB MIDI RX: reconstruct SysEx from byte stream ----
    uint8_t sysex_build_[512]{};
    size_t sysex_build_len_ = 0;
    bool in_sysex_ = false;

    // Ring buffer for incoming SysEx messages (allows queueing multiple)
    static constexpr size_t kSysexQueueSlots = 4;
    static constexpr size_t kSysexSlotSize = 512;
    uint8_t sysex_queue_[kSysexQueueSlots][kSysexSlotSize]{};
    size_t sysex_queue_len_[kSysexQueueSlots]{};
    volatile size_t sysex_queue_head_ = 0; // Next slot to write
    volatile size_t sysex_queue_tail_ = 0; // Next slot to read
};
