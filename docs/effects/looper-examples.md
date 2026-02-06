# Looper Effect - Usage Examples

This file demonstrates how to use the looper effect in different scenarios.

## Example 1: Simple Recording and Playback

```cpp
#include "effects/looper.h"
#include "audio/audio_processor.h"

// Setup
LooperEffect looper;
float bufL[LooperEffect::MAX_SAMPLES];
float bufR[LooperEffect::MAX_SAMPLES];

looper.BindBuffers(bufL, bufR);
looper.Init(48000.0f);

// Set playback level to 100%
looper.SetParam(0, 1.0f);  // Level = 100%

// Set feedback to 80% for overdubbing later
looper.SetParam(1, 0.8f);  // Feedback = 80%

// Start recording
looper.SetParam(2, 0.3f);  // Action = 30% (Record range: 20-40%)

// ... process audio frames during recording ...
for (int i = 0; i < recordingSamples; i++) {
    float l = inputSignalL[i];
    float r = inputSignalR[i];
    looper.ProcessStereo(l, r);
    outputL[i] = l;
    outputR[i] = r;
}

// Stop recording and start playback
looper.SetParam(2, 0.3f);  // Action = 30% (second trigger stops recording)

// ... process audio frames during playback ...
for (int i = 0; i < playbackSamples; i++) {
    float l = drySignalL[i];
    float r = drySignalR[i];
    looper.ProcessStereo(l, r);  // Mixes loop playback with input
    outputL[i] = l;
    outputR[i] = r;
}
```

## Example 2: Overdubbing with Feedback Control

```cpp
// Start with a playing loop (from Example 1)

// Switch to overdub mode
looper.SetParam(2, 0.7f);  // Action = 70% (Overdub range: 60-80%)

// Adjust feedback for layering behavior
// 100% = full retention (layers accumulate)
// 50% = equal mix of old and new
// 0% = complete replacement
looper.SetParam(1, 0.9f);  // Feedback = 90% (strong layer retention)

// ... process audio frames during overdubbing ...
for (int i = 0; i < overdubSamples; i++) {
    float l = newLayerL[i];
    float r = newLayerR[i];
    looper.ProcessStereo(l, r);  // Adds new layer to existing loop
    outputL[i] = l;
    outputR[i] = r;
}

// Stop overdubbing and return to playback
looper.SetParam(2, 0.7f);  // Action = 70% (second trigger stops overdubbing)
```

## Example 3: Using Output Parameters for UI Feedback

```cpp
// Get current looper state for UI display
OutputParamDesc outputs[3];
uint8_t numOutputs = looper.GetOutputParams(outputs, 3);

if (numOutputs >= 3) {
    // Output param 0 (id=3): State
    int state = (int)outputs[0].value;
    const char* stateNames[] = {"Idle", "Recording", "Playing", "Overdubbing"};
    printf("State: %s\n", stateNames[state]);
    
    // Output param 1 (id=4): Loop Length
    float loopLength = outputs[1].value;
    printf("Loop Length: %.2f seconds\n", loopLength);
    
    // Output param 2 (id=5): Position
    float position = outputs[2].value;
    printf("Position: %.1f%%\n", position * 100.0f);
}
```

## Example 4: Footswitch Integration

```cpp
// Map footswitch to cycle through states
// Footswitch 1: Record/Play/Overdub cycle
// Footswitch 2: Clear

bool footswitch1Pressed = false;
bool footswitch2Pressed = false;

void OnFootswitchPressed(int switchNum) {
    if (switchNum == 1) {
        if (!footswitch1Pressed) {
            footswitch1Pressed = true;
            
            // Get current state
            OutputParamDesc outputs[3];
            looper.GetOutputParams(outputs, 3);
            int currentState = (int)outputs[0].value;
            
            switch (currentState) {
                case 0:  // Idle -> Start Recording
                    looper.SetParam(2, 0.3f);  // Action = 30% (Record)
                    break;
                case 1:  // Recording -> Stop and Play
                    looper.SetParam(2, 0.3f);  // Action = 30% (stops recording)
                    break;
                case 2:  // Playing -> Overdub
                    looper.SetParam(2, 0.7f);  // Action = 70% (Overdub)
                    break;
                case 3:  // Overdubbing -> Back to Playing
                    looper.SetParam(2, 0.7f);  // Action = 70% (stops overdub)
                    break;
            }
        }
    }
    else if (switchNum == 2) {
        if (!footswitch2Pressed) {
            footswitch2Pressed = true;
            // Clear loop
            looper.SetParam(2, 0.9f);  // Action = 90% (Clear)
        }
    }
}

void OnFootswitchReleased(int switchNum) {
    if (switchNum == 1) {
        footswitch1Pressed = false;
    } else if (switchNum == 2) {
        footswitch2Pressed = false;
    }
}
```

## Example 5: MIDI SysEx Control

```cpp
// MIDI CC mapping for looper parameters
// CC 20: Level (0-127 -> 0-100%)
// CC 21: Feedback (0-127 -> 0-100%)
// CC 22: Action (0-127 -> 0-100%)

void HandleMidiCC(uint8_t cc, uint8_t value) {
    float normalized = value / 127.0f;
    
    switch (cc) {
        case 20:  // Level
            looper.SetParam(0, normalized);
            break;
        case 21:  // Feedback
            looper.SetParam(1, normalized);
            break;
        case 22:  // Action
            looper.SetParam(2, normalized);
            break;
    }
}

// Example MIDI messages:
// Record:    CC 22, value 32  (25% of 127 = 32)
// Play:      CC 22, value 64  (50% of 127 = 64)
// Overdub:   CC 22, value 89  (70% of 127 = 89)
// Clear:     CC 22, value 114 (90% of 127 = 114)
```

## Example 6: Integration in AudioProcessor Patch

```cpp
#include "patch/patch_protocol.h"
#include "audio/audio_processor.h"

// Create a patch with looper in slot 5
PatchWireDesc patch;
patch.numSlots = 6;

// Slot 0: Noise Gate
patch.slots[0].typeId = NoiseGateEffect::TypeId;
patch.slots[0].enabled = 1;
patch.slots[0].inputL = ROUTE_INPUT;
patch.slots[0].inputR = ROUTE_INPUT;

// Slot 1-4: Other effects (compressor, overdrive, etc.)
// ...

// Slot 5: Looper
patch.slots[5].typeId = LooperEffect::TypeId;  // TypeId = 28
patch.slots[5].enabled = 1;
patch.slots[5].inputL = 4;  // Take from previous slot
patch.slots[5].inputR = 4;
patch.slots[5].sumToMono = 0;
patch.slots[5].dry = 0;     // 0% dry (only processed signal)
patch.slots[5].wet = 127;   // 100% wet
patch.slots[5].channelPolicy = (uint8_t)ChannelPolicy::Auto;
patch.slots[5].numParams = 3;

// Set initial parameters
patch.slots[5].params[0].id = 0;
patch.slots[5].params[0].value = 127;  // Level = 100%
patch.slots[5].params[1].id = 1;
patch.slots[5].params[1].value = 102;  // Feedback = 80%
patch.slots[5].params[2].id = 2;
patch.slots[5].params[2].value = 0;    // Action = Idle

// Apply patch to audio processor
AudioProcessor processor(tempoSource);
processor.ApplyPatch(patch);
```

## Example 7: Advanced - Variable Feedback During Overdub

```cpp
// Dynamically adjust feedback based on input level
// High input = lower feedback (prevent buildup)
// Low input = higher feedback (maintain loop)

float maxInputLevel = 0.0f;
const float LEVEL_DECAY = 0.99f;

// During overdub mode processing
for (int i = 0; i < blockSize; i++) {
    float l = inputL[i];
    float r = inputR[i];
    
    // Track input level
    float inputLevel = std::max(std::abs(l), std::abs(r));
    if (inputLevel > maxInputLevel) {
        maxInputLevel = inputLevel;
    } else {
        maxInputLevel *= LEVEL_DECAY;
    }
    
    // Adjust feedback based on input level
    // High input (>0.5) = 60% feedback
    // Low input (<0.1) = 95% feedback
    float targetFeedback = 0.95f - (maxInputLevel * 0.35f);
    looper.SetParam(1, targetFeedback);
    
    looper.ProcessStereo(l, r);
    outputL[i] = l;
    outputR[i] = r;
}
```

## Example 8: Performance Monitoring

```cpp
#include <chrono>

// Measure CPU usage of looper
void MeasureLooperPerformance() {
    const int TEST_SAMPLES = 48000;  // 1 second
    float testL[TEST_SAMPLES];
    float testR[TEST_SAMPLES];
    
    // Fill with test signal
    for (int i = 0; i < TEST_SAMPLES; i++) {
        testL[i] = 0.5f * sin(2.0f * M_PI * 440.0f * i / 48000.0f);
        testR[i] = testL[i];
    }
    
    // Start recording
    looper.SetParam(2, 0.3f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Process samples
    for (int i = 0; i < TEST_SAMPLES; i++) {
        float l = testL[i];
        float r = testR[i];
        looper.ProcessStereo(l, r);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    printf("Processed %d samples in %lld µs\n", TEST_SAMPLES, duration.count());
    printf("Average: %.2f µs per sample\n", duration.count() / (float)TEST_SAMPLES);
    printf("CPU usage at 48kHz: %.2f%%\n", 
           (duration.count() / (float)TEST_SAMPLES) / (1000000.0f / 48000.0f) * 100.0f);
}
```

## Tips and Best Practices

1. **Always bind buffers before Init()**: The looper requires external SDRAM buffers
2. **Use edge detection for Action parameter**: Don't continuously set the same action value
3. **Monitor output parameters**: Use GetOutputParams() to sync UI with looper state
4. **Feedback range**: 70-90% is ideal for most overdubbing scenarios
5. **Clear before new recording**: Use Clear action to start fresh
6. **Memory awareness**: Only one looper instance can be active at a time
7. **Check state before actions**: Some actions only work in specific states (e.g., Overdub requires a loop)

## Common Pitfalls

1. **Forgetting to bind buffers**: Results in null pointer access
2. **Setting action continuously**: Use edge detection to avoid rapid state changes
3. **Feedback = 100% with hot signal**: Can cause gradual volume increase
4. **Feedback = 0%**: Effectively replaces loop instead of layering
5. **Not clearing before new session**: Previous loop content remains in buffer
