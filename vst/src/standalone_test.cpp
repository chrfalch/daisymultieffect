// Standalone test for the core DSP library.
// This allows testing the effect chain without a DAW.

#include "core/audio/audio_processor.h"
#include "core/audio/tempo.h"
#include "core/patch/patch_protocol.h"
#include "core/effects/effect_registry.h"
#include "BufferManager.h"

#include <cstdio>
#include <cmath>
#include <vector>

// Generate a simple test signal (sine wave)
void GenerateTestSignal(std::vector<float> &buffer, float frequency, float sampleRate)
{
    for (size_t i = 0; i < buffer.size(); i++)
    {
        buffer[i] = 0.5f * std::sin(2.0f * 3.14159265f * frequency * i / sampleRate);
    }
}

// Create a simple test patch with distortion and delay
PatchWireDesc CreateTestPatch()
{
    PatchWireDesc patch{};
    patch.numSlots = 2;

    // Slot 0: Distortion
    patch.slots[0].slotIndex = 0;
    patch.slots[0].typeId = 10; // DistortionEffect::TypeId
    patch.slots[0].enabled = 1;
    patch.slots[0].inputL = ROUTE_INPUT;
    patch.slots[0].inputR = ROUTE_INPUT;
    patch.slots[0].sumToMono = 0;
    patch.slots[0].dry = 0;
    patch.slots[0].wet = 127;
    patch.slots[0].channelPolicy = 0;
    patch.slots[0].numParams = 2;
    patch.slots[0].params[0] = {0, 64}; // Drive at 50%
    patch.slots[0].params[1] = {1, 64}; // Tone at 50%

    // Slot 1: Delay
    patch.slots[1].slotIndex = 1;
    patch.slots[1].typeId = 1; // DelayEffect::TypeId
    patch.slots[1].enabled = 1;
    patch.slots[1].inputL = 0; // From slot 0
    patch.slots[1].inputR = 0;
    patch.slots[1].sumToMono = 0;
    patch.slots[1].dry = 32;
    patch.slots[1].wet = 96;
    patch.slots[1].channelPolicy = 0;
    patch.slots[1].numParams = 5;
    patch.slots[1].params[0] = {0, 64};  // Free time
    patch.slots[1].params[1] = {1, 32};  // Division
    patch.slots[1].params[2] = {2, 127}; // Synced
    patch.slots[1].params[3] = {3, 64};  // Feedback
    patch.slots[1].params[4] = {4, 96};  // Mix

    return patch;
}

int main()
{
    printf("DaisyMultiFX Core DSP Test\n");
    printf("==========================\n\n");

    // Setup
    const float sampleRate = 48000.0f;
    const size_t blockSize = 256;
    const size_t numBlocks = 100;

    // Create tempo source
    TempoSource tempo;
    tempo.bpm = 120.0f;
    tempo.valid = true;

    // Create processor and buffer manager
    AudioProcessor processor(tempo);
    BufferManager buffers;

    // Bind buffers and initialize
    buffers.BindTo(processor);
    processor.Init(sampleRate);

    // Print available effects
    printf("Available effects:\n");
    const uint8_t typeIds[] = {1, 10, 12, 13, 14, 15, 16};
    for (uint8_t id : typeIds)
    {
        const EffectMeta *meta = EffectRegistry::Lookup(id);
        if (meta)
        {
            printf("  [%d] %s - %s\n", id, meta->name, meta->description);
        }
    }
    printf("\n");

    // Apply test patch
    PatchWireDesc patch = CreateTestPatch();
    processor.ApplyPatch(patch);
    printf("Applied test patch with %d slots\n\n", patch.numSlots);

    // Generate test signal
    std::vector<float> inputL(blockSize), inputR(blockSize);
    std::vector<float> outputL(blockSize), outputR(blockSize);
    GenerateTestSignal(inputL, 440.0f, sampleRate);
    inputR = inputL; // Mono input

    // Process audio
    printf("Processing %zu blocks of %zu samples...\n", numBlocks, blockSize);

    float peakIn = 0.0f, peakOut = 0.0f;

    for (size_t block = 0; block < numBlocks; block++)
    {
        for (size_t i = 0; i < blockSize; i++)
        {
            float l, r;
            processor.ProcessFrame(inputL[i], inputR[i], l, r);
            outputL[i] = l;
            outputR[i] = r;

            peakIn = std::fmax(peakIn, std::fabs(inputL[i]));
            peakOut = std::fmax(peakOut, std::fmax(std::fabs(l), std::fabs(r)));
        }
    }

    printf("\nResults:\n");
    printf("  Peak input level:  %.3f\n", peakIn);
    printf("  Peak output level: %.3f\n", peakOut);
    printf("  Processing complete!\n");

    return 0;
}
