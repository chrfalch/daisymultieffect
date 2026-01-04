#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "core/audio/audio_processor.h"
#include "core/audio/tempo.h"
#include "core/state/patch_state.h"
#include "core/protocol/midi_protocol.h"
#include "BufferManager.h"
#include <atomic>

// Number of effect slots exposed in the UI/parameter system
static constexpr int kNumSlots = 12;

// Default patch configuration for the guitar signal chain
// Maximum params per effect slot (should match largest effect)
static constexpr int kMaxParamsPerSlot = 7;

struct DefaultSlotConfig
{
    uint8_t typeId;                  // Effect type ID
    int typeIndex;                   // Index in kAllEffects array for JUCE combo box
    float params[kMaxParamsPerSlot]; // Normalized parameters (0-1)
};

// Guitar signal chain: Gate -> Compressor -> Distortion -> EQ -> Chorus -> Reverb
inline const DefaultSlotConfig kDefaultSlots[kNumSlots] = {
    // Slot 0: Noise Gate (typeId=17, index=8)
    {17, 8, {64.0f / 127.0f, 20.0f / 127.0f, 50.0f / 127.0f, 40.0f / 127.0f, 0.0f / 127.0f, 0.5f, 0.5f}},
    // Slot 1: Compressor (typeId=15, index=6)
    {15, 6, {80.0f / 127.0f, 16.0f / 127.0f, 40.0f / 127.0f, 50.0f / 127.0f, 20.0f / 127.0f, 0.5f, 0.5f}},
    // Slot 2: Distortion (typeId=10, index=2)
    {10, 2, {40.0f / 127.0f, 70.0f / 127.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
    // Slot 3: GraphicEQ (typeId=18, index=9) - 7 bands
    {18, 9, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
    // Slot 4: Chorus (typeId=16, index=7)
    {16, 7, {30.0f / 127.0f, 50.0f / 127.0f, 20.0f / 127.0f, 40.0f / 127.0f, 50.0f / 127.0f, 0.5f, 0.5f}},
    // Slot 5: Reverb (typeId=14, index=5)
    {14, 5, {40.0f / 127.0f, 50.0f / 127.0f, 60.0f / 127.0f, 25.0f / 127.0f, 50.0f / 127.0f, 0.5f, 0.5f}},
};

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
    void onSlotRoutingChanged(uint8_t slot, uint8_t inputL, uint8_t inputR) override;
    void onSlotSumToMonoChanged(uint8_t slot, bool sumToMono) override;
    void onSlotChannelPolicyChanged(uint8_t slot, uint8_t channelPolicy) override;
    void onPatchLoaded() override;
    void onTempoChanged(float bpm) override;

    // Accessors
    juce::AudioProcessorValueTreeState &getValueTreeState() { return parameters_; }
    daisyfx::PatchState &getPatchState() { return patchState_; }
    float getInputLevel() const { return inputLevel_.load(); }
    float getOutputLevel() const { return outputLevel_.load(); }

    // Access to audio processor for Neural Amp model loading
    CoreAudioProcessor *getAudioProcessor() { return processor_.get(); }

    // Neural Amp model loading - returns true if successful
    bool loadNeuralAmpModel(int slotIndex, const juce::File &modelFile);

    // Get the current Neural Amp model name for a slot
    juce::String getNeuralAmpModelName(int slotIndex) const;

    // Cabinet IR loading - returns true if successful
    bool loadCabinetIR(int slotIndex, const juce::File &irFile);

    // Get the current Cabinet IR name for a slot
    juce::String getCabinetIRName(int slotIndex) const;

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
