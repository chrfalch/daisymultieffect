#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/audio/audio_processor.h"
#include "core/audio/tempo.h"
#include "core/patch/patch_protocol.h"
#include "BufferManager.h"

// Rename to avoid conflict with juce::AudioProcessor
using CoreAudioProcessor = ::AudioProcessor;

class DaisyMultiFXProcessor : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener
{
public:
    DaisyMultiFXProcessor();
    ~DaisyMultiFXProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // Parameter listener
    void parameterChanged(const juce::String &parameterID, float newValue) override;

    // Access for editor
    juce::AudioProcessorValueTreeState &getValueTreeState() { return parameters_; }

    // Level metering
    float getInputLevel() const { return inputLevel_.load(); }
    float getOutputLevel() const { return outputLevel_.load(); }

    // Patch management
    void applyPatch(const PatchWireDesc &patch);
    void setSlotEnabled(int slotIndex, bool enabled);
    void setSlotParam(int slotIndex, int paramId, float value);

    // SysEx handling
    void handleSysEx(const uint8_t *data, int size);

    // Send SysEx responses (patch dump, effect meta)
    void sendPatchDump(juce::MidiBuffer &midiOut);
    void sendEffectMeta(juce::MidiBuffer &midiOut);

private:
    // Core DSP
    TempoSource tempo_;
    std::unique_ptr<CoreAudioProcessor> processor_;
    std::unique_ptr<BufferManager> buffers_;
    bool isPrepared_ = false;

    // Pending MIDI output (for SysEx responses)
    juce::MidiBuffer pendingMidiOut_;

    // Current patch
    PatchWireDesc currentPatch_;

    // Level metering (atomic for thread-safe access from UI)
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};

    // Parameters
    juce::AudioProcessorValueTreeState parameters_;

    // Create parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Initialize default patch
    void initializeDefaultPatch();

    // Sync GUI parameters from current patch
    void syncParametersFromPatch();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DaisyMultiFXProcessor)
};
