#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/effects/effect_registry.h"
#include <iostream>

DaisyMultiFXProcessor::DaisyMultiFXProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, juce::Identifier("DaisyMultiFX"), createParameterLayout())
{
    // Create DSP components
    processor_ = std::make_unique<CoreAudioProcessor>(tempo_);
    buffers_ = std::make_unique<BufferManager>();

    // Bind buffers to processor
    buffers_->BindTo(*processor_);

    // Initialize with default patch
    initializeDefaultPatch();

    // Sync GUI parameters to match default patch
    syncParametersFromPatch();

    // Add parameter listeners
    parameters_.addParameterListener("tempo", this);
    for (int slot = 0; slot < 4; ++slot)
    {
        parameters_.addParameterListener("slot" + juce::String(slot) + "_enabled", this);
        parameters_.addParameterListener("slot" + juce::String(slot) + "_type", this);
        parameters_.addParameterListener("slot" + juce::String(slot) + "_mix", this);
        for (int p = 0; p < 5; ++p)
        {
            parameters_.addParameterListener("slot" + juce::String(slot) + "_p" + juce::String(p), this);
        }
    }
}

DaisyMultiFXProcessor::~DaisyMultiFXProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DaisyMultiFXProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global tempo
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tempo", 1),
        "Tempo",
        juce::NormalisableRange<float>(20.0f, 300.0f, 0.1f),
        120.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int)
        { return juce::String(value, 1) + " BPM"; }));

    // Per-slot parameters (4 slots for GUI simplicity)
    for (int slot = 0; slot < 4; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);

        // Enabled
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "_enabled", 1),
            "Slot " + juce::String(slot + 1) + " Enabled",
            true));

        // Effect type (0=Off, 1=Delay, 10=Distortion, etc.)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "_type", 1),
            "Slot " + juce::String(slot + 1) + " Type",
            juce::StringArray{"Off", "Delay", "Distortion", "Sweep Delay", "Mixer", "Reverb", "Compressor", "Chorus"},
            0));

        // Mix (wet/dry)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "_mix", 1),
            "Slot " + juce::String(slot + 1) + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            1.0f));

        // Generic parameters (5 per slot)
        for (int p = 0; p < 5; ++p)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(prefix + "_p" + juce::String(p), 1),
                "Slot " + juce::String(slot + 1) + " Param " + juce::String(p + 1),
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                0.5f));
        }
    }

    return {params.begin(), params.end()};
}

void DaisyMultiFXProcessor::initializeDefaultPatch()
{
    currentPatch_ = {};
    currentPatch_.numSlots = 4;

    // Slot 0: Distortion (enabled by default) - mono effect
    currentPatch_.slots[0].slotIndex = 0;
    currentPatch_.slots[0].typeId = 10; // Distortion
    currentPatch_.slots[0].enabled = 1;
    currentPatch_.slots[0].inputL = ROUTE_INPUT;
    currentPatch_.slots[0].inputR = ROUTE_INPUT;
    currentPatch_.slots[0].sumToMono = 1;  // Sum to mono for mono effect
    currentPatch_.slots[0].dry = 0;
    currentPatch_.slots[0].wet = 127;
    currentPatch_.slots[0].channelPolicy = 0;
    currentPatch_.slots[0].numParams = 2;
    currentPatch_.slots[0].params[0] = {0, 40}; // Drive ~30%
    currentPatch_.slots[0].params[1] = {1, 64}; // Tone 50%

    // Slot 1: Delay
    currentPatch_.slots[1].slotIndex = 1;
    currentPatch_.slots[1].typeId = 1; // Delay
    currentPatch_.slots[1].enabled = 1;
    currentPatch_.slots[1].inputL = 0; // From slot 0
    currentPatch_.slots[1].inputR = 0;
    currentPatch_.slots[1].sumToMono = 0;
    currentPatch_.slots[1].dry = 64;
    currentPatch_.slots[1].wet = 96;
    currentPatch_.slots[1].channelPolicy = 0;
    currentPatch_.slots[1].numParams = 5;
    currentPatch_.slots[1].params[0] = {0, 64};  // Free time
    currentPatch_.slots[1].params[1] = {1, 32};  // Division
    currentPatch_.slots[1].params[2] = {2, 127}; // Synced
    currentPatch_.slots[1].params[3] = {3, 50};  // Feedback
    currentPatch_.slots[1].params[4] = {4, 80};  // Mix

    // Slot 2: Reverb
    currentPatch_.slots[2].slotIndex = 2;
    currentPatch_.slots[2].typeId = 14; // Reverb
    currentPatch_.slots[2].enabled = 1;
    currentPatch_.slots[2].inputL = 1; // From slot 1
    currentPatch_.slots[2].inputR = 1;
    currentPatch_.slots[2].sumToMono = 0;
    currentPatch_.slots[2].dry = 64;
    currentPatch_.slots[2].wet = 80;
    currentPatch_.slots[2].channelPolicy = 0;
    currentPatch_.slots[2].numParams = 5;
    currentPatch_.slots[2].params[0] = {0, 50}; // Mix
    currentPatch_.slots[2].params[1] = {1, 70}; // Decay
    currentPatch_.slots[2].params[2] = {2, 40}; // Damp
    currentPatch_.slots[2].params[3] = {3, 30}; // Pre-delay
    currentPatch_.slots[2].params[4] = {4, 64}; // Size

    // Slot 3: Off (available for user)
    currentPatch_.slots[3].slotIndex = 3;
    currentPatch_.slots[3].typeId = 0;
    currentPatch_.slots[3].enabled = 1;
    currentPatch_.slots[3].inputL = 2; // From slot 2
    currentPatch_.slots[3].inputR = 2;
    currentPatch_.slots[3].sumToMono = 0;
    currentPatch_.slots[3].dry = 0;
    currentPatch_.slots[3].wet = 127;
    currentPatch_.slots[3].channelPolicy = 0;
    currentPatch_.slots[3].numParams = 0;

    // Set default tempo
    tempo_.bpm = 120.0f;
    tempo_.valid = true;
}

void DaisyMultiFXProcessor::syncParametersFromPatch()
{
    // Map typeId to combo index
    auto typeIdToIndex = [](uint8_t typeId) -> int
    {
        switch (typeId)
        {
        case 0:
            return 0; // Off
        case 1:
            return 1; // Delay
        case 10:
            return 2; // Distortion
        case 12:
            return 3; // Sweep Delay
        case 13:
            return 4; // Mixer
        case 14:
            return 5; // Reverb
        case 15:
            return 6; // Compressor
        case 16:
            return 7; // Chorus
        default:
            return 0;
        }
    };

    for (int slot = 0; slot < 4; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);
        const auto &slotData = currentPatch_.slots[slot];

        // Set type parameter using getRawParameterValue for direct access
        if (auto *typeParam = dynamic_cast<juce::AudioParameterChoice *>(parameters_.getParameter(prefix + "_type")))
        {
            int typeIndex = typeIdToIndex(slotData.typeId);
            *parameters_.getRawParameterValue(prefix + "_type") = static_cast<float>(typeIndex);
        }

        // Set enabled
        if (auto *enabledParam = parameters_.getParameter(prefix + "_enabled"))
        {
            *parameters_.getRawParameterValue(prefix + "_enabled") = slotData.enabled ? 1.0f : 0.0f;
        }

        // Set mix (wet amount normalized)
        if (auto *mixParam = parameters_.getParameter(prefix + "_mix"))
        {
            *parameters_.getRawParameterValue(prefix + "_mix") = slotData.wet / 127.0f;
        }

        // Set parameters from slot params
        for (int p = 0; p < 5; ++p)
        {
            juce::String paramId = prefix + "_p" + juce::String(p);
            if (auto *param = parameters_.getParameter(paramId))
            {
                float value = 0.5f; // default
                if (p < slotData.numParams)
                {
                    value = slotData.params[p].value / 127.0f;
                }
                *parameters_.getRawParameterValue(paramId) = value;
            }
        }
    }

    // Set tempo
    if (auto *tempoParam = parameters_.getParameter("tempo"))
    {
        // Normalize 120 BPM to 0-1 range (20-300 BPM range)
        float normalizedTempo = (tempo_.bpm - 20.0f) / (300.0f - 20.0f);
        *parameters_.getRawParameterValue("tempo") = normalizedTempo;
    }
}

void DaisyMultiFXProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    processor_->Init(static_cast<float>(sampleRate));
    processor_->ApplyPatch(currentPatch_);
    isPrepared_ = true;
}

void DaisyMultiFXProcessor::releaseResources()
{
    isPrepared_ = false;
}

bool DaisyMultiFXProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void DaisyMultiFXProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Debug: Check buffer configuration
    static int debugCount = 0;
    if (++debugCount % 1000 == 1) // Log every ~20 seconds at 48kHz/512 samples
    {
        std::cerr << "processBlock: buffer channels=" << buffer.getNumChannels()
                  << " samples=" << buffer.getNumSamples()
                  << " isPrepared=" << isPrepared_
                  << " processor=" << (processor_ ? "valid" : "NULL") << std::endl;
    }
    
    // Safety check
    if (!isPrepared_ || !processor_)
        return;

    // Get tempo from host if available
    if (auto *playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (auto bpm = posInfo->getBpm())
            {
                tempo_.bpm = static_cast<float>(*bpm);
                tempo_.valid = true;
            }
        }
    }

    // Process MIDI for tempo/control - handle SysEx messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isSysEx())
        {
            handleSysEx(message.getSysExData(), message.getSysExDataSize());
        }
    }

    // Add any pending MIDI output (SysEx responses)
    if (!pendingMidiOut_.isEmpty())
    {
        midiMessages.addEvents(pendingMidiOut_, 0, buffer.getNumSamples(), 0);
        pendingMidiOut_.clear();
    }

    // Process audio - handle mono input gracefully
    const int numInputChannels = buffer.getNumChannels();
    auto *leftIn = buffer.getReadPointer(0);
    auto *rightIn = (numInputChannels > 1) ? buffer.getReadPointer(1) : leftIn; // Duplicate mono to stereo
    auto *leftOut = buffer.getWritePointer(0);
    auto *rightOut = (numInputChannels > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    float maxIn = 0.0f, maxOut = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inL = leftIn[i], inR = rightIn[i];
        float inMax = std::max(std::abs(inL), std::abs(inR));
        if (inMax > maxIn)
            maxIn = inMax;

        float outL, outR;
        processor_->ProcessFrame(inL, inR, outL, outR);
        leftOut[i] = outL;
        rightOut[i] = outR;

        float outMax = std::max(std::abs(outL), std::abs(outR));
        if (outMax > maxOut)
            maxOut = outMax;
    }
    
    // Debug: Log levels periodically
    if (debugCount % 1000 == 1 && (maxIn > 0.001f || maxOut > 0.001f))
    {
        std::cerr << "Audio levels: maxIn=" << maxIn << " maxOut=" << maxOut << std::endl;
    }

    // Update level meters with smoothing
    const float attack = 0.9f;
    const float release = 0.97f;

    float currentIn = inputLevel_.load();
    inputLevel_.store(maxIn > currentIn ? maxIn : currentIn * release);

    float currentOut = outputLevel_.load();
    outputLevel_.store(maxOut > currentOut ? maxOut : currentOut * release);
}

void DaisyMultiFXProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == "tempo")
    {
        tempo_.bpm = newValue;
        tempo_.valid = true;
        return;
    }

    // Parse slot parameters
    if (parameterID.startsWith("slot"))
    {
        int slotIndex = parameterID.substring(4, 5).getIntValue();
        if (slotIndex < 0 || slotIndex >= 4)
            return;

        juce::String suffix = parameterID.substring(5);

        if (suffix == "_enabled")
        {
            setSlotEnabled(slotIndex, newValue > 0.5f);
        }
        else if (suffix == "_mix")
        {
            currentPatch_.slots[slotIndex].wet = static_cast<uint8_t>(newValue * 127.0f);
            if (isPrepared_)
                processor_->ApplyPatch(currentPatch_);
        }
        else if (suffix == "_type")
        {
            // Map choice index to type ID
            const uint8_t typeIds[] = {0, 1, 10, 12, 13, 14, 15, 16};
            int typeIndex = static_cast<int>(newValue);
            if (typeIndex >= 0 && typeIndex < 8)
            {
                auto &slot = currentPatch_.slots[slotIndex];
                slot.typeId = typeIds[typeIndex];

                // Initialize default parameters for the effect type
                switch (slot.typeId)
                {
                case 1: // Delay
                    slot.numParams = 5;
                    slot.params[0] = {0, 64};  // Free time
                    slot.params[1] = {1, 32};  // Division
                    slot.params[2] = {2, 127}; // Synced
                    slot.params[3] = {3, 50};  // Feedback
                    slot.params[4] = {4, 80};  // Mix
                    break;
                case 10: // Distortion (mono effect)
                    slot.numParams = 2;
                    slot.sumToMono = 1;  // Distortion is a mono effect
                    slot.params[0] = {0, 40}; // Drive
                    slot.params[1] = {1, 64}; // Tone
                    break;
                case 12: // Sweep Delay
                    slot.numParams = 7;
                    slot.params[0] = {0, 64};  // Free time
                    slot.params[1] = {1, 32};  // Division
                    slot.params[2] = {2, 127}; // Synced
                    slot.params[3] = {3, 50};  // Feedback
                    slot.params[4] = {4, 80};  // Mix
                    slot.params[5] = {5, 64};  // Pan depth
                    slot.params[6] = {6, 32};  // Pan rate
                    break;
                case 13: // Mixer
                    slot.numParams = 3;
                    slot.params[0] = {0, 64}; // Mix A
                    slot.params[1] = {1, 64}; // Mix B
                    slot.params[2] = {2, 0};  // Cross
                    break;
                case 14: // Reverb
                    slot.numParams = 5;
                    slot.params[0] = {0, 50}; // Mix
                    slot.params[1] = {1, 70}; // Decay
                    slot.params[2] = {2, 40}; // Damp
                    slot.params[3] = {3, 30}; // Pre-delay
                    slot.params[4] = {4, 64}; // Size
                    break;
                case 15: // Compressor
                    slot.numParams = 5;
                    slot.params[0] = {0, 64}; // Threshold
                    slot.params[1] = {1, 32}; // Ratio
                    slot.params[2] = {2, 32}; // Attack
                    slot.params[3] = {3, 64}; // Release
                    slot.params[4] = {4, 32}; // Makeup
                    break;
                case 16: // Chorus
                    slot.numParams = 5;
                    slot.params[0] = {0, 40}; // Rate
                    slot.params[1] = {1, 64}; // Depth
                    slot.params[2] = {2, 0};  // Feedback
                    slot.params[3] = {3, 64}; // Delay
                    slot.params[4] = {4, 64}; // Mix
                    break;
                default:
                    slot.numParams = 0;
                    break;
                }

                if (isPrepared_)
                    processor_->ApplyPatch(currentPatch_);
            }
        }
        else if (suffix.startsWith("_p"))
        {
            int paramId = suffix.substring(2).getIntValue();
            setSlotParam(slotIndex, paramId, newValue);
        }
    }
}

void DaisyMultiFXProcessor::applyPatch(const PatchWireDesc &patch)
{
    currentPatch_ = patch;
    if (isPrepared_)
        processor_->ApplyPatch(currentPatch_);
}

void DaisyMultiFXProcessor::setSlotEnabled(int slotIndex, bool enabled)
{
    if (slotIndex >= 0 && slotIndex < 12)
    {
        processor_->Board().slots[slotIndex].enabled = enabled;
    }
}

void DaisyMultiFXProcessor::setSlotParam(int slotIndex, int paramId, float value)
{
    if (slotIndex >= 0 && slotIndex < 12)
    {
        auto &slot = processor_->Board().slots[slotIndex];
        if (slot.effect)
        {
            slot.effect->SetParam(static_cast<uint8_t>(paramId), value);
        }

        // Also update the patch struct for persistence
        auto &sw = currentPatch_.slots[slotIndex];
        for (int i = 0; i < sw.numParams && i < 8; ++i)
        {
            if (sw.params[i].id == paramId)
            {
                sw.params[i].value = static_cast<uint8_t>(value * 127.0f);
                return;
            }
        }
        // Add new param if not found
        if (sw.numParams < 8)
        {
            sw.params[sw.numParams].id = static_cast<uint8_t>(paramId);
            sw.params[sw.numParams].value = static_cast<uint8_t>(value * 127.0f);
            sw.numParams++;
        }
    }
}

juce::AudioProcessorEditor *DaisyMultiFXProcessor::createEditor()
{
    return new DaisyMultiFXEditor(*this);
}

bool DaisyMultiFXProcessor::hasEditor() const
{
    return true;
}

const juce::String DaisyMultiFXProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DaisyMultiFXProcessor::acceptsMidi() const
{
    return true;
}

bool DaisyMultiFXProcessor::producesMidi() const
{
    return true; // We output SysEx responses
}

bool DaisyMultiFXProcessor::isMidiEffect() const
{
    return false;
}

double DaisyMultiFXProcessor::getTailLengthSeconds() const
{
    return 2.0; // For delay/reverb tails
}

int DaisyMultiFXProcessor::getNumPrograms()
{
    return 1;
}

int DaisyMultiFXProcessor::getCurrentProgram()
{
    return 0;
}

void DaisyMultiFXProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String DaisyMultiFXProcessor::getProgramName(int /*index*/)
{
    return {};
}

void DaisyMultiFXProcessor::changeProgramName(int /*index*/, const juce::String & /*newName*/)
{
}

void DaisyMultiFXProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = parameters_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DaisyMultiFXProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(parameters_.state.getType()))
    {
        parameters_.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// SysEx message handling - compatible with macOS control app
// Uses shared protocol from core/protocol/sysex_protocol.h
void DaisyMultiFXProcessor::handleSysEx(const uint8_t *data, int size)
{
    // SysEx format: F0 7D <cmd> <data...> F7
    // JUCE strips F0 and F7, so we get: 7D <cmd> <data...>

    if (size < 2)
        return;

    if (data[0] != MANUFACTURER_ID)
        return;

    uint8_t cmd = data[1];

    DBG("SysEx received: cmd=0x" << juce::String::toHexString(cmd) << " size=" << size);

    switch (cmd)
    {
    case SysExCmd::SET_PARAM: // Set parameter: 7D 20 <slot> <paramId> <value>
        if (size >= 5)
        {
            uint8_t slot = data[2];
            uint8_t paramId = data[3];
            uint8_t value = data[4];

            if (slot < MAX_SLOTS && paramId < MAX_PARAMS_PER_SLOT)
            {
                DBG("SysEx SetParam: slot=" << (int)slot << " param=" << (int)paramId << " value=" << (int)value);

                // Update patch and GUI
                if (slot < 4 && paramId < 5)
                {
                    juce::String prefix = "slot" + juce::String(slot);
                    if (auto *param = parameters_.getParameter(prefix + "_p" + juce::String(paramId)))
                    {
                        param->setValueNotifyingHost(value / 127.0f);
                    }
                }

                // Update internal patch
                if (slot < currentPatch_.numSlots)
                {
                    for (int i = 0; i < currentPatch_.slots[slot].numParams; ++i)
                    {
                        if (currentPatch_.slots[slot].params[i].id == paramId)
                        {
                            currentPatch_.slots[slot].params[i].value = value;
                            break;
                        }
                    }
                    if (isPrepared_)
                        processor_->ApplyPatch(currentPatch_);
                }
            }
        }
        break;

    case SysExCmd::SET_ENABLED: // Set slot enabled: 7D 21 <slot> <enabled>
        if (size >= 4)
        {
            uint8_t slot = data[2];
            bool enabled = data[3] != 0;

            if (slot < MAX_SLOTS)
            {
                DBG("SysEx SetEnabled: slot=" << (int)slot << " enabled=" << (enabled ? "yes" : "no"));

                // Update GUI for slots 0-3
                if (slot < 4)
                {
                    juce::String prefix = "slot" + juce::String(slot);
                    if (auto *param = parameters_.getParameter(prefix + "_enabled"))
                    {
                        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
                    }
                }

                // Update internal state
                currentPatch_.slots[slot].enabled = enabled ? 1 : 0;
                setSlotEnabled(slot, enabled);
            }
        }
        break;

    case SysExCmd::SET_TYPE: // Set slot type: 7D 22 <slot> <typeId>
        if (size >= 4)
        {
            uint8_t slot = data[2];
            uint8_t typeId = data[3];

            if (slot < MAX_SLOTS)
            {
                DBG("SysEx SetType: slot=" << (int)slot << " typeId=" << (int)typeId);

                // Map typeId to combo index for GUI
                auto typeIdToIndex = [](uint8_t tid) -> int
                {
                    switch (tid)
                    {
                    case EffectType::OFF:
                        return 0; // Off
                    case EffectType::DELAY:
                        return 1; // Delay
                    case EffectType::DISTORTION:
                        return 2; // Distortion
                    case EffectType::SWEEP_DELAY:
                        return 3; // Sweep Delay
                    case EffectType::MIXER:
                        return 4; // Mixer
                    case EffectType::REVERB:
                        return 5; // Reverb
                    case EffectType::COMPRESSOR:
                        return 6; // Compressor
                    case EffectType::CHORUS:
                        return 7; // Chorus
                    default:
                        return 0;
                    }
                };

                // Update GUI for slots 0-3
                if (slot < 4)
                {
                    juce::String prefix = "slot" + juce::String(slot);
                    if (auto *typeParam = dynamic_cast<juce::AudioParameterChoice *>(
                            parameters_.getParameter(prefix + "_type")))
                    {
                        int idx = typeIdToIndex(typeId);
                        *parameters_.getRawParameterValue(prefix + "_type") = static_cast<float>(idx);
                    }
                }

                // Update internal patch
                currentPatch_.slots[slot].typeId = typeId;
                if (isPrepared_)
                    processor_->ApplyPatch(currentPatch_);
            }
        }
        break;

    case SysExCmd::REQUEST_PATCH: // Request patch dump - respond with current patch
        DBG("SysEx RequestPatch - sending patch dump");
        sendPatchDump(pendingMidiOut_);
        break;

    case SysExCmd::REQUEST_EFFECT_META: // Request effect meta
        DBG("SysEx RequestMeta - sending effect metadata");
        sendEffectMeta(pendingMidiOut_);
        break;

    default:
        DBG("SysEx: Unknown command 0x" << juce::String::toHexString(cmd));
        break;
    }
}

// Send patch dump response matching firmware format:
// F0 7D 13 <numSlots>
//   12x slots: slotIndex typeId enabled inputL inputR sumToMono dry wet policy numParams (id val)x8
//   2x buttons: slotIndex mode
// F7
void DaisyMultiFXProcessor::sendPatchDump(juce::MidiBuffer &midiOut)
{
    std::vector<uint8_t> sysex;
    sysex.reserve(512);
    sysex.push_back(0xF0);
    sysex.push_back(MANUFACTURER_ID);
    sysex.push_back(SysExResp::PATCH_DUMP);
    sysex.push_back(currentPatch_.numSlots & 0x7F);

    // Always send 12 slots (matching firmware protocol)
    for (uint8_t i = 0; i < MAX_SLOTS; ++i)
    {
        const auto &slot = currentPatch_.slots[i];
        sysex.push_back(i & 0x7F); // slotIndex
        sysex.push_back(slot.typeId & 0x7F);
        sysex.push_back(slot.enabled & 0x7F);
        sysex.push_back(encodeRoute(slot.inputL)); // inputL (route encoded)
        sysex.push_back(encodeRoute(slot.inputR)); // inputR (route encoded)
        sysex.push_back(slot.sumToMono & 0x7F);
        sysex.push_back(slot.dry & 0x7F);
        sysex.push_back(slot.wet & 0x7F);
        sysex.push_back(slot.channelPolicy & 0x7F); // policy
        sysex.push_back(slot.numParams & 0x7F);

        // Always send 8 param pairs (matching firmware protocol)
        for (uint8_t p = 0; p < MAX_PARAMS_PER_SLOT; ++p)
        {
            sysex.push_back(slot.params[p].id & 0x7F);
            sysex.push_back(slot.params[p].value & 0x7F);
        }
    }

    // 2x button mappings (send empty/default since VST doesn't use hardware buttons)
    for (int b = 0; b < NUM_BUTTONS; ++b)
    {
        sysex.push_back(127); // slotIndex = 127 means unassigned
        sysex.push_back(0);   // mode = 0
    }

    sysex.push_back(0xF7);

    auto msg = juce::MidiMessage::createSysExMessage(sysex.data() + 1, (int)sysex.size() - 2);
    midiOut.addEvent(msg, 0);

    DBG("Sent patch dump: " << sysex.size() << " bytes");
}

// Send effect meta response: F0 7D 33 <numEffects> [<effect data>...] F7
void DaisyMultiFXProcessor::sendEffectMeta(juce::MidiBuffer &midiOut)
{
    // Effect type info: typeId, nameLen, name, numParams, [paramInfo...]
    struct EffectInfo
    {
        uint8_t typeId;
        const char *name;
        std::vector<std::pair<uint8_t, const char *>> params; // id, name
    };

    std::vector<EffectInfo> effects = {
        {EffectType::OFF, "Off", {}},
        {EffectType::DELAY, "Delay", {{0, "Time"}, {1, "Division"}, {2, "Sync"}, {3, "Feedback"}, {4, "Mix"}}},
        {EffectType::DISTORTION, "Distortion", {{0, "Drive"}, {1, "Tone"}}},
        {EffectType::SWEEP_DELAY, "Sweep Delay", {{0, "Time"}, {1, "Division"}, {2, "Sync"}, {3, "Feedback"}, {4, "Mix"}, {5, "Pan Depth"}, {6, "Pan Rate"}}},
        {EffectType::MIXER, "Mixer", {{0, "Mix A"}, {1, "Mix B"}, {2, "Cross"}}},
        {EffectType::REVERB, "Reverb", {{0, "Mix"}, {1, "Decay"}, {2, "Damp"}, {3, "Pre-delay"}, {4, "Size"}}},
        {EffectType::COMPRESSOR, "Compressor", {{0, "Threshold"}, {1, "Ratio"}, {2, "Attack"}, {3, "Release"}, {4, "Makeup"}}},
        {EffectType::CHORUS, "Chorus", {{0, "Rate"}, {1, "Depth"}, {2, "Feedback"}, {3, "Delay"}, {4, "Mix"}}}};

    std::vector<uint8_t> sysex;
    sysex.push_back(0xF0);
    sysex.push_back(MANUFACTURER_ID);
    sysex.push_back(SysExResp::EFFECT_META_LIST);
    sysex.push_back(static_cast<uint8_t>(effects.size()));

    for (const auto &fx : effects)
    {
        sysex.push_back(fx.typeId);

        // Name (length-prefixed, ASCII only, 7-bit safe)
        size_t nameLen = strlen(fx.name);
        sysex.push_back(static_cast<uint8_t>(nameLen));
        for (size_t c = 0; c < nameLen; ++c)
            sysex.push_back(static_cast<uint8_t>(fx.name[c]) & 0x7F);

        // Params
        sysex.push_back(static_cast<uint8_t>(fx.params.size()));
        for (const auto &param : fx.params)
        {
            sysex.push_back(param.first); // param id
            size_t pnameLen = strlen(param.second);
            sysex.push_back(static_cast<uint8_t>(pnameLen));
            for (size_t c = 0; c < pnameLen; ++c)
                sysex.push_back(static_cast<uint8_t>(param.second[c]) & 0x7F);
        }
    }

    sysex.push_back(0xF7);

    auto msg = juce::MidiMessage::createSysExMessage(sysex.data() + 1, (int)sysex.size() - 2);
    midiOut.addEvent(msg, 0);

    DBG("Sent effect meta: " << sysex.size() << " bytes");
}

// Plugin instantiation
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DaisyMultiFXProcessor();
}
