#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "core/audio/audio_processor.h"
#include "core/audio/tempo.h"
#include "core/state/patch_state.h"
#include "core/protocol/midi_protocol.h"
#include "BufferManager.h"
#include <atomic>

// Number of effect slots exposed in the UI/parameter system
static constexpr int kNumSlots = 6;

// Rename to avoid conflict with juce::AudioProcessor
using CoreAudioProcessor = ::AudioProcessor;

/**
 * DaisyMultiFXProcessor - VST/AU Plugin Processor
 *
 * Architecture:
 * - PatchState is the single source of truth
 * - This class observes PatchState to update DSP and broadcast MIDI
 * - UI changes go through PatchState (not directly to MIDI)
 * - Incoming MIDI commands go to PatchState (which deduplicates and notifies)
 */
class DaisyMultiFXProcessor : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener,
                              public daisyfx::PatchObserver
{
public:
    DaisyMultiFXProcessor();
    ~DaisyMultiFXProcessor() override;

    // AudioProcessor interface
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

    // Parameter listener (for APVTS -> PatchState)
    void parameterChanged(const juce::String &parameterID, float newValue) override;

    // PatchObserver interface (for PatchState -> DSP/MIDI/UI)
    void onSlotEnabledChanged(uint8_t slot, bool enabled) override;
    void onSlotTypeChanged(uint8_t slot, uint8_t typeId) override;
    void onSlotParamChanged(uint8_t slot, uint8_t paramId, uint8_t value) override;
    void onSlotMixChanged(uint8_t slot, uint8_t wet, uint8_t dry) override;
    void onPatchLoaded() override;
    void onTempoChanged(float bpm) override;

    // Accessors
    juce::AudioProcessorValueTreeState &getValueTreeState() { return parameters_; }
    daisyfx::PatchState &getPatchState() { return patchState_; }
    float getInputLevel() const { return inputLevel_.load(); }
    float getOutputLevel() const { return outputLevel_.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void initializeDefaultPatch();
    void syncParametersFromPatch(); // PatchState -> APVTS
    void syncPatchFromParameters(); // APVTS -> PatchState (after state restore)

    // MIDI handling
    void handleIncomingMidi(const juce::MidiMessage &message);
    void sendPatchDump();
    void sendEffectMeta();

    // Core state - THE source of truth
    daisyfx::PatchState patchState_;

    // DSP
    std::unique_ptr<CoreAudioProcessor> processor_;
    std::unique_ptr<BufferManager> buffers_;
    TempoSource tempo_;
    bool isPrepared_ = false;

    // JUCE parameter system (for DAW automation)
    juce::AudioProcessorValueTreeState parameters_;

    // MIDI output buffer
    juce::MidiBuffer pendingMidiOut_;

    // Rate limiting for MIDI responses
    double lastPatchDumpTime_ = 0.0;
    double lastEffectMetaTime_ = 0.0;
    static constexpr double kMinResponseIntervalMs = 500.0; // 500ms between responses

    // Level meters
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};

    // Flag to prevent circular updates (APVTS -> PatchState -> APVTS)
    bool isUpdatingFromPatchState_ = false;
    bool isUpdatingFromAPVTS_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DaisyMultiFXProcessor)
};
