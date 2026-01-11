#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <memory>

/**
 * MidiBridge - Routes MIDI between two devices bidirectionally
 *
 * Used to bridge Network MIDI (iPad) <-> USB MIDI (Daisy Seed)
 */
class MidiBridge : public juce::MidiInputCallback
{
public:
    MidiBridge();
    ~MidiBridge() override;

    // Device management
    static juce::StringArray getAvailableInputs();
    static juce::StringArray getAvailableOutputs();

    // Connect to devices by name (partial match supported)
    bool connectInput(const juce::String &deviceNamePattern);
    bool connectOutput(const juce::String &deviceNamePattern);
    void disconnectAll();

    // Check connection status
    bool isInputConnected() const { return midiInput_ != nullptr; }
    bool isOutputConnected() const { return midiOutput_ != nullptr; }
    juce::String getInputName() const { return inputName_; }
    juce::String getOutputName() const { return outputName_; }

    // Statistics
    int getMessageCount() const { return messageCount_.load(); }
    void resetMessageCount() { messageCount_ = 0; }

    // Optional callback for monitoring
    std::function<void(const juce::MidiMessage &, bool isIncoming)> onMidiMessage;

private:
    void handleIncomingMidiMessage(juce::MidiInput *source,
                                   const juce::MidiMessage &message) override;

    std::unique_ptr<juce::MidiInput> midiInput_;
    std::unique_ptr<juce::MidiOutput> midiOutput_;
    juce::String inputName_;
    juce::String outputName_;
    std::atomic<int> messageCount_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBridge)
};

/**
 * BidirectionalMidiBridge - Two MidiBridges for full bidirectional routing
 *
 * Routes: DeviceA -> DeviceB AND DeviceB -> DeviceA
 */
class BidirectionalMidiBridge
{
public:
    BidirectionalMidiBridge();
    ~BidirectionalMidiBridge();

    // Connect two devices for bidirectional communication
    bool connectDevices(const juce::String &deviceAPattern,
                        const juce::String &deviceBPattern);

    void disconnect();

    bool isConnected() const;
    bool isPartiallyConnected() const;
    juce::String getDeviceAName() const { return bridgeAtoB_.getInputName(); }
    juce::String getDeviceBName() const { return bridgeBtoA_.getInputName(); }

    // Statistics
    int getAtoBCount() const { return bridgeAtoB_.getMessageCount(); }
    int getBtoACount() const { return bridgeBtoA_.getMessageCount(); }
    void resetCounts()
    {
        bridgeAtoB_.resetMessageCount();
        bridgeBtoA_.resetMessageCount();
    }

    // Optional callback for monitoring all traffic
    std::function<void(const juce::MidiMessage &, const juce::String &from, const juce::String &to)> onMidiRouted;

private:
    MidiBridge bridgeAtoB_; // A input -> B output
    MidiBridge bridgeBtoA_; // B input -> A output

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BidirectionalMidiBridge)
};
