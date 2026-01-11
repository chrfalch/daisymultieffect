
#include "patch/patch_protocol.h"
#include "midi/midi_control.h"
#include "audio/tempo_control.h"
#include "audio/audio_processor.h"
#include "patch/ui_control.h"

#include "daisy_seed.h"
#include "util/CpuLoadMeter.h"

#include <algorithm>
#include <cmath>

static daisy::DaisySeed g_hw;
static MidiControl g_midi;

static TempoSource g_tempo;
static TempoControl g_tempoControl;
static AudioProcessor g_processor(g_tempo);
static UiControl g_ui;

// CPU load metering
static daisy::CpuLoadMeter g_cpuMeter;

// Level metering (updated per-block in audio callback, read by main loop)
static volatile float g_inputLevel = 0.0f;
static volatile float g_outputLevel = 0.0f;

// Buffer binding helper - called at startup to bind SDRAM buffers
extern void BindProcessorBuffers(AudioProcessor &processor);

PatchWireDesc MakeExamplePatch();
PatchWireDesc MakePassthroughPatch();

// Mono input detection: copies left channel to right when right is silent
// This handles the common case of a mono guitar input on a stereo codec
static void AudioCallback(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t size)
{
    g_cpuMeter.OnBlockStart();

    g_midi.ApplyPendingInAudioThread();

    // Detect mono input: check if right channel is essentially silent while left has signal
    float leftEnergy = 0.0f, rightEnergy = 0.0f;
    for (size_t i = 0; i < size; ++i)
    {
        leftEnergy += in[0][i] * in[0][i];
        rightEnergy += in[1][i] * in[1][i];
    }

    // If left has significant signal but right is near-silent, treat as mono
    bool monoInput = (leftEnergy > 1e-8f && rightEnergy < leftEnergy * 0.001f);

    if (monoInput)
    {
        // Process with left channel duplicated to right
        for (size_t i = 0; i < size; ++i)
        {
            float inL = in[0][i];
            float outL, outR;
            g_processor.ProcessFrame(inL, inL, outL, outR);
            out[0][i] = outL;
            out[1][i] = outR;
        }
    }
    else
    {
        // Normal stereo processing
        g_processor.ProcessBlock(in, out, size);
    }

    // Level metering: use processor's post-gain peak levels
    float maxIn = g_processor.GetInputPeakLevel();
    float maxOut = g_processor.GetOutputPeakLevel();
    g_processor.ResetPeakLevels();

    // Apply release smoothing (ballistics) - done once per block, not per sample
    constexpr float release = 0.97f;
    float curIn = g_inputLevel;
    float curOut = g_outputLevel;
    g_inputLevel = maxIn > curIn ? maxIn : curIn * release;
    g_outputLevel = maxOut > curOut ? maxOut : curOut * release;

    g_cpuMeter.OnBlockEnd();
}

int main()
{
    g_hw.Init();

    BindProcessorBuffers(g_processor);
    g_processor.Init(g_hw.AudioSampleRate());
    g_midi.Init(g_hw, g_processor, g_processor.Board(), g_tempo);
    g_tempoControl.Init(g_tempo, &g_midi);

    g_ui.Init(g_hw, g_processor, g_midi, g_tempoControl);

    constexpr size_t kBlockSize = 48;
    g_hw.SetAudioBlockSize(kBlockSize);

    // Initialize CPU load meter
    g_cpuMeter.Init(g_hw.AudioSampleRate(), kBlockSize);

    auto pw =
#if defined(DEBUG_PATCH_PASSTHROUGH)
        MakePassthroughPatch();
#else
        MakeExamplePatch();
#endif
    g_ui.SetPatch(pw);

    g_hw.StartAudio(AudioCallback);

    // Timing for status updates and CPU meter reset
    uint32_t lastStatusMs = 0;
    uint32_t lastCpuResetMs = 0;
    constexpr uint32_t kStatusIntervalMs = 100;    // ~4Hz status updates
    constexpr uint32_t kCpuResetIntervalMs = 5000; // Reset CPU max every 5 seconds

    while (true)
    {
        uint32_t nowMs = daisy::System::GetNow();

        g_ui.Process();
        g_midi.Process();

        // Send status update at ~30Hz
        if (nowMs - lastStatusMs >= kStatusIntervalMs)
        {
            lastStatusMs = nowMs;
            g_midi.SendStatusUpdate(
                g_inputLevel,
                g_outputLevel,
                g_cpuMeter.GetAvgCpuLoad(),
                g_cpuMeter.GetMaxCpuLoad());
        }

        // Reset CPU max periodically to keep it meaningful ("recent peak")
        if (nowMs - lastCpuResetMs >= kCpuResetIntervalMs)
        {
            lastCpuResetMs = nowMs;
            g_cpuMeter.Reset();
        }

        g_hw.DelayMs(10);
    }
}
