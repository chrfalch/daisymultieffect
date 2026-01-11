#include "MidiBridge.h"

//==============================================================================
// MidiBridge Implementation
//==============================================================================

MidiBridge::MidiBridge() = default;

MidiBridge::~MidiBridge()
{
    disconnectAll();
}

juce::StringArray MidiBridge::getAvailableInputs()
{
    juce::StringArray names;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& device : devices)
    {
        names.add(device.name);
    }
    return names;
}

juce::StringArray MidiBridge::getAvailableOutputs()
{
    juce::StringArray names;
    auto devices = juce::MidiOutput::getAvailableDevices();
    for (const auto& device : devices)
    {
        names.add(device.name);
    }
    return names;
}

bool MidiBridge::connectInput(const juce::String& deviceNamePattern)
{
    auto devices = juce::MidiInput::getAvailableDevices();
    
    for (const auto& device : devices)
    {
        if (device.name.containsIgnoreCase(deviceNamePattern))
        {
            midiInput_ = juce::MidiInput::openDevice(device.identifier, this);
            if (midiInput_)
            {
                inputName_ = device.name;
                midiInput_->start();
                DBG("MidiBridge: Connected input: " + device.name);
                return true;
            }
        }
    }
    
    DBG("MidiBridge: No input found matching: " + deviceNamePattern);
    return false;
}

bool MidiBridge::connectOutput(const juce::String& deviceNamePattern)
{
    auto devices = juce::MidiOutput::getAvailableDevices();
    
    for (const auto& device : devices)
    {
        if (device.name.containsIgnoreCase(deviceNamePattern))
        {
            midiOutput_ = juce::MidiOutput::openDevice(device.identifier);
            if (midiOutput_)
            {
                outputName_ = device.name;
                DBG("MidiBridge: Connected output: " + device.name);
                return true;
            }
        }
    }
    
    DBG("MidiBridge: No output found matching: " + deviceNamePattern);
    return false;
}

void MidiBridge::disconnectAll()
{
    if (midiInput_)
    {
        midiInput_->stop();
        midiInput_.reset();
        inputName_.clear();
    }
    
    midiOutput_.reset();
    outputName_.clear();
}

void MidiBridge::handleIncomingMidiMessage(juce::MidiInput* /*source*/, 
                                            const juce::MidiMessage& message)
{
    // Forward to output
    if (midiOutput_)
    {
        midiOutput_->sendMessageNow(message);
        messageCount_++;
    }
    
    // Notify callback if set
    if (onMidiMessage)
    {
        onMidiMessage(message, true);
    }
}

//==============================================================================
// BidirectionalMidiBridge Implementation
//==============================================================================

BidirectionalMidiBridge::BidirectionalMidiBridge() = default;

BidirectionalMidiBridge::~BidirectionalMidiBridge()
{
    disconnect();
}

bool BidirectionalMidiBridge::connectDevices(const juce::String& deviceAPattern, 
                                              const juce::String& deviceBPattern)
{
    // Set up A -> B routing
    bool aInputOk = bridgeAtoB_.connectInput(deviceAPattern);
    bool bOutputOk = bridgeAtoB_.connectOutput(deviceBPattern);
    
    // Set up B -> A routing  
    bool bInputOk = bridgeBtoA_.connectInput(deviceBPattern);
    bool aOutputOk = bridgeBtoA_.connectOutput(deviceAPattern);
    
    // Set up monitoring callbacks
    bridgeAtoB_.onMidiMessage = [this](const juce::MidiMessage& msg, bool /*isIncoming*/) {
        if (onMidiRouted)
        {
            onMidiRouted(msg, bridgeAtoB_.getInputName(), bridgeAtoB_.getOutputName());
        }
    };
    
    bridgeBtoA_.onMidiMessage = [this](const juce::MidiMessage& msg, bool /*isIncoming*/) {
        if (onMidiRouted)
        {
            onMidiRouted(msg, bridgeBtoA_.getInputName(), bridgeBtoA_.getOutputName());
        }
    };
    
    if (aInputOk && bOutputOk && bInputOk && aOutputOk)
    {
        DBG("BidirectionalMidiBridge: Full bidirectional connection established");
        DBG("  " + bridgeAtoB_.getInputName() + " <-> " + bridgeBtoA_.getInputName());
        return true;
    }
    
    // Partial connection - log what worked
    DBG("BidirectionalMidiBridge: Partial connection:");
    DBG("  A->B: input=" + juce::String(aInputOk ? "OK" : "FAIL") + 
        ", output=" + juce::String(bOutputOk ? "OK" : "FAIL"));
    DBG("  B->A: input=" + juce::String(bInputOk ? "OK" : "FAIL") + 
        ", output=" + juce::String(aOutputOk ? "OK" : "FAIL"));
    
    return aInputOk || bOutputOk || bInputOk || aOutputOk;
}

void BidirectionalMidiBridge::disconnect()
{
    bridgeAtoB_.disconnectAll();
    bridgeBtoA_.disconnectAll();
}

bool BidirectionalMidiBridge::isConnected() const
{
    return bridgeAtoB_.isInputConnected() && bridgeAtoB_.isOutputConnected() &&
           bridgeBtoA_.isInputConnected() && bridgeBtoA_.isOutputConnected();
}

bool BidirectionalMidiBridge::isPartiallyConnected() const
{
    return (bridgeAtoB_.isInputConnected() || bridgeAtoB_.isOutputConnected() ||
            bridgeBtoA_.isInputConnected() || bridgeBtoA_.isOutputConnected()) &&
           !isConnected();
}
