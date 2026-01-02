
#include "patch/patch_protocol.h"
#include "midi/midi_control.h"
#include "audio/tempo_control.h"
#include "audio/audio_processor.h"
#include "patch/ui_control.h"

#include "daisy_seed.h"

static daisy::DaisySeed g_hw;
static MidiControl g_midi;

static TempoSource g_tempo;
static TempoControl g_tempoControl;
static AudioProcessor g_processor(g_tempo);
static UiControl g_ui;

// Buffer binding helper - called at startup to bind SDRAM buffers
extern void BindProcessorBuffers(AudioProcessor &processor);

PatchWireDesc MakeExamplePatch();
PatchWireDesc MakePassthroughPatch();
static void AudioCallback(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t size)
{
    g_midi.ApplyPendingInAudioThread();

    g_processor.ProcessBlock(in, out, size);
}

int main()
{
    g_hw.Init();

    BindProcessorBuffers(g_processor);
    g_processor.Init(g_hw.AudioSampleRate());
    g_midi.Init(g_hw, g_processor, g_processor.Board(), g_tempo);
    g_tempoControl.Init(g_tempo, &g_midi);

    g_ui.Init(g_hw, g_processor, g_midi, g_tempoControl);

    g_hw.SetAudioBlockSize(48);

    auto pw =
#if defined(DEBUG_PATCH_PASSTHROUGH)
        MakePassthroughPatch();
#else
        MakeExamplePatch();
#endif
    g_ui.SetPatch(pw);

    g_hw.StartAudio(AudioCallback);

    while (true)
    {
        g_ui.Process();
        g_midi.Process();
        g_hw.DelayMs(10);
    }
}
