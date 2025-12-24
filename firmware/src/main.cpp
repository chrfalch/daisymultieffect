
#include "patch_protocol.h"
#include "midi_control.h"
#include "tempo_control.h"
#include "audio_engine.h"
#include "ui_control.h"

#include "daisy_seed.h"

static daisy::DaisySeed g_hw;
static MidiControl g_midi;

static TempoSource g_tempo;
static TempoControl g_tempoControl;
static AudioEngine g_engine(g_tempo);
static UiControl g_ui;

PatchWireDesc MakeExamplePatch();
static void AudioCallback(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t size)
{
    g_midi.ApplyPendingParamInAudioThread();

    g_engine.ProcessBlock(in, out, size);
}

int main()
{
    g_hw.Init();

    g_engine.Init(g_hw.AudioSampleRate());
    g_midi.Init(g_hw, g_engine.Board(), g_tempo);
    g_tempoControl.Init(g_tempo, &g_midi);

    g_ui.Init(g_hw, g_engine, g_midi, g_tempoControl);

    g_hw.SetAudioBlockSize(48);

    auto pw = MakeExamplePatch();
    g_ui.SetPatch(pw);

    g_hw.StartAudio(AudioCallback);

    while (true)
    {
        g_ui.Process();
        g_midi.Process();
        g_hw.DelayMs(10);
    }
}
