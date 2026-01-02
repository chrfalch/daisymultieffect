#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/effects/effect_registry.h"
#include <iostream>

using namespace daisyfx;

DaisyMultiFXProcessor::DaisyMultiFXProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, juce::Identifier("DaisyMultiFX"), createParameterLayout())
{
    // Register as observer of PatchState
    patchState_.addObserver(this);

    // Create DSP components
    processor_ = std::make_unique<CoreAudioProcessor>(tempo_);
    buffers_ = std::make_unique<BufferManager>();
    buffers_->BindTo(*processor_);

    // Initialize with default patch
    initializeDefaultPatch();

    // Sync JUCE parameters to match initial PatchState
    syncParametersFromPatch();

    // Add parameter listeners (APVTS -> PatchState)
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
    patchState_.removeObserver(this);
}

juce::AudioProcessorValueTreeState::ParameterLayout DaisyMultiFXProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global tempo
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tempo", 1), "Tempo",
        juce::NormalisableRange<float>(20.0f, 300.0f, 0.1f), 120.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int)
        { return juce::String(value, 1) + " BPM"; }));

    // Per-slot parameters (4 slots for GUI)
    for (int slot = 0; slot < 4; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "_enabled", 1),
            "Slot " + juce::String(slot + 1) + " Enabled", true));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "_type", 1),
            "Slot " + juce::String(slot + 1) + " Type",
            juce::StringArray{"Off", "Delay", "Distortion", "Sweep Delay", "Mixer", "Reverb", "Compressor", "Chorus"},
            0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "_mix", 1),
            "Slot " + juce::String(slot + 1) + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        for (int p = 0; p < 5; ++p)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(prefix + "_p" + juce::String(p), 1),
                "Slot " + juce::String(slot + 1) + " Param " + juce::String(p + 1),
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        }
    }

    return {params.begin(), params.end()};
}

void DaisyMultiFXProcessor::initializeDefaultPatch()
{
    // Create default patch and load into PatchState
    PatchWireDesc patch = {};
    patch.numSlots = 4;

    // Slot 0: Distortion
    patch.slots[0].slotIndex = 0;
    patch.slots[0].typeId = MidiProtocol::EffectType::DISTORTION;
    patch.slots[0].enabled = 1;
    patch.slots[0].inputL = ROUTE_INPUT;
    patch.slots[0].inputR = ROUTE_INPUT;
    patch.slots[0].sumToMono = 1;
    patch.slots[0].wet = 127;
    patch.slots[0].numParams = 2;
    patch.slots[0].params[0] = {0, 40};
    patch.slots[0].params[1] = {1, 64};

    // Slot 1: Delay
    patch.slots[1].slotIndex = 1;
    patch.slots[1].typeId = MidiProtocol::EffectType::DELAY;
    patch.slots[1].enabled = 1;
    patch.slots[1].inputL = 0;
    patch.slots[1].inputR = 0;
    patch.slots[1].dry = 64;
    patch.slots[1].wet = 96;
    patch.slots[1].numParams = 5;
    patch.slots[1].params[0] = {0, 64};
    patch.slots[1].params[1] = {1, 32};
    patch.slots[1].params[2] = {2, 127};
    patch.slots[1].params[3] = {3, 50};
    patch.slots[1].params[4] = {4, 80};

    // Slot 2: Reverb
    patch.slots[2].slotIndex = 2;
    patch.slots[2].typeId = MidiProtocol::EffectType::REVERB;
    patch.slots[2].enabled = 1;
    patch.slots[2].inputL = 1;
    patch.slots[2].inputR = 1;
    patch.slots[2].dry = 64;
    patch.slots[2].wet = 80;
    patch.slots[2].numParams = 5;
    patch.slots[2].params[0] = {0, 50};
    patch.slots[2].params[1] = {1, 70};
    patch.slots[2].params[2] = {2, 40};
    patch.slots[2].params[3] = {3, 30};
    patch.slots[2].params[4] = {4, 64};

    // Slot 3: Off
    patch.slots[3].slotIndex = 3;
    patch.slots[3].typeId = MidiProtocol::EffectType::OFF;
    patch.slots[3].enabled = 1;
    patch.slots[3].inputL = 2;
    patch.slots[3].inputR = 2;
    patch.slots[3].wet = 127;

    // Load into PatchState (will notify observers)
    patchState_.loadPatch(patch);
    tempo_.bpm = 120.0f;
    tempo_.valid = true;
}

void DaisyMultiFXProcessor::syncParametersFromPatch()
{
    isUpdatingFromPatchState_ = true;

    auto typeIdToIndex = [](uint8_t typeId) -> int
    {
        switch (typeId)
        {
        case 0:
            return 0;
        case 1:
            return 1;
        case 10:
            return 2;
        case 12:
            return 3;
        case 13:
            return 4;
        case 14:
            return 5;
        case 15:
            return 6;
        case 16:
            return 7;
        default:
            return 0;
        }
    };

    const auto &patch = patchState_.getPatch();

    for (int slot = 0; slot < 4; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);
        const auto &slotData = patch.slots[slot];

        *parameters_.getRawParameterValue(prefix + "_type") = static_cast<float>(typeIdToIndex(slotData.typeId));
        *parameters_.getRawParameterValue(prefix + "_enabled") = slotData.enabled ? 1.0f : 0.0f;
        *parameters_.getRawParameterValue(prefix + "_mix") = slotData.wet / 127.0f;

        for (int p = 0; p < 5; ++p)
        {
            float value = 0.5f;
            if (p < slotData.numParams)
                value = slotData.params[p].value / 127.0f;
            *parameters_.getRawParameterValue(prefix + "_p" + juce::String(p)) = value;
        }
    }

    isUpdatingFromPatchState_ = false;
}

//=============================================================================
// Parameter Listener (APVTS -> PatchState)
//=============================================================================

void DaisyMultiFXProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    // Avoid circular updates
    if (isUpdatingFromPatchState_)
        return;

    if (parameterID == "tempo")
    {
        patchState_.setTempo(newValue);
        return;
    }

    if (parameterID.startsWith("slot"))
    {
        int slotIndex = parameterID.substring(4, 5).getIntValue();
        if (slotIndex < 0 || slotIndex >= 4)
            return;

        juce::String suffix = parameterID.substring(5);

        if (suffix == "_enabled")
        {
            patchState_.setSlotEnabled(static_cast<uint8_t>(slotIndex), newValue > 0.5f);
        }
        else if (suffix == "_mix")
        {
            uint8_t wet = static_cast<uint8_t>(newValue * 127.0f);
            patchState_.setSlotMix(static_cast<uint8_t>(slotIndex), wet, 0);
        }
        else if (suffix == "_type")
        {
            const uint8_t typeIds[] = {0, 1, 10, 12, 13, 14, 15, 16};
            int typeIndex = static_cast<int>(newValue);
            if (typeIndex >= 0 && typeIndex < 8)
                patchState_.setSlotType(static_cast<uint8_t>(slotIndex), typeIds[typeIndex]);
        }
        else if (suffix.startsWith("_p"))
        {
            int paramId = suffix.substring(2).getIntValue();
            uint8_t value = static_cast<uint8_t>(newValue * 127.0f);
            patchState_.setSlotParam(static_cast<uint8_t>(slotIndex), static_cast<uint8_t>(paramId), value);
        }
    }
}

//=============================================================================
// PatchObserver callbacks (PatchState -> DSP/MIDI)
//=============================================================================

void DaisyMultiFXProcessor::onSlotEnabledChanged(uint8_t slot, bool enabled)
{
    // Update DSP
    if (slot < 12 && processor_)
        processor_->Board().slots[slot].enabled = enabled;

    // Update APVTS if needed (for UI)
    if (!isUpdatingFromPatchState_ && slot < 4)
    {
        isUpdatingFromPatchState_ = true;
        if (auto *param = parameters_.getParameter("slot" + juce::String(slot) + "_enabled"))
            param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        isUpdatingFromPatchState_ = false;
    }
}

void DaisyMultiFXProcessor::onSlotTypeChanged(uint8_t slot, uint8_t typeId)
{
    // Update DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());

    // Update APVTS
    if (!isUpdatingFromPatchState_ && slot < 4)
    {
        isUpdatingFromPatchState_ = true;
        auto typeIdToIndex = [](uint8_t tid) -> int
        {
            switch (tid)
            {
            case 0:
                return 0;
            case 1:
                return 1;
            case 10:
                return 2;
            case 12:
                return 3;
            case 13:
                return 4;
            case 14:
                return 5;
            case 15:
                return 6;
            case 16:
                return 7;
            default:
                return 0;
            }
        };
        if (auto *param = dynamic_cast<juce::AudioParameterChoice *>(
                parameters_.getParameter("slot" + juce::String(slot) + "_type")))
        {
            float normalizedValue = typeIdToIndex(typeId) / static_cast<float>(param->choices.size() - 1);
            param->setValueNotifyingHost(normalizedValue);
        }
        isUpdatingFromPatchState_ = false;
    }
}

void DaisyMultiFXProcessor::onSlotParamChanged(uint8_t slot, uint8_t paramId, uint8_t value)
{
    // Update DSP directly
    if (slot < 12 && processor_)
    {
        auto &dspSlot = processor_->Board().slots[slot];
        if (dspSlot.effect)
            dspSlot.effect->SetParam(paramId, value / 127.0f);
    }

    // Update APVTS
    if (!isUpdatingFromPatchState_ && slot < 4 && paramId < 5)
    {
        isUpdatingFromPatchState_ = true;
        juce::String paramName = "slot" + juce::String(slot) + "_p" + juce::String(paramId);
        if (auto *param = parameters_.getParameter(paramName))
            param->setValueNotifyingHost(value / 127.0f);
        isUpdatingFromPatchState_ = false;
    }
}

void DaisyMultiFXProcessor::onSlotMixChanged(uint8_t slot, uint8_t wet, uint8_t dry)
{
    // Update DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());
}

void DaisyMultiFXProcessor::onPatchLoaded()
{
    // Full patch reload - update DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());

    // Sync APVTS
    syncParametersFromPatch();
}

void DaisyMultiFXProcessor::onTempoChanged(float bpm)
{
    tempo_.bpm = bpm;
    tempo_.valid = true;
}

//=============================================================================
// MIDI Handling
//=============================================================================

void DaisyMultiFXProcessor::handleIncomingMidi(const juce::MidiMessage &message)
{
    if (!message.isSysEx())
        return;

    auto *data = message.getSysExData();
    int size = message.getSysExDataSize();

    // Need at least manufacturer + sender + cmd
    if (size < 3 || data[0] != MidiProtocol::MANUFACTURER_ID)
        return;

    uint8_t sender = data[1];
    uint8_t cmd = data[2];

    // Ignore our own messages (prevents loopback via IAC)
    if (sender == MidiProtocol::Sender::VST)
        return;

    std::cout << "[VST] Received cmd=0x" << std::hex << (int)cmd
              << " from sender=0x" << (int)sender << std::dec << std::endl;

    auto decoded = MidiProtocol::decode(data, size);
    if (!decoded.valid)
    {
        std::cout << "[VST] Decoded invalid for cmd=0x" << std::hex << (int)cmd << std::dec << std::endl;
        return;
    }

    // Route commands to PatchState - it handles deduplication
    switch (decoded.command)
    {
    case MidiProtocol::Cmd::SET_ENABLED:
        patchState_.setSlotEnabled(decoded.slot, decoded.enabled);
        break;

    case MidiProtocol::Cmd::SET_TYPE:
        patchState_.setSlotType(decoded.slot, decoded.typeId);
        break;

    case MidiProtocol::Cmd::SET_PARAM:
        patchState_.setSlotParam(decoded.slot, decoded.paramId, decoded.value);
        break;

    case MidiProtocol::Cmd::REQUEST_PATCH:
        sendPatchDump();
        break;

    case MidiProtocol::Cmd::REQUEST_META:
        sendEffectMeta();
        break;
    }
}

void DaisyMultiFXProcessor::sendPatchDump()
{
    // Rate limit responses
    double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastPatchDumpTime_ < kMinResponseIntervalMs)
    {
        std::cout << "[VST] sendPatchDump rate-limited" << std::endl;
        return;
    }
    lastPatchDumpTime_ = now;

    std::cout << "[VST] Sending PATCH_DUMP response" << std::endl;
    auto sysex = MidiProtocol::encodePatchDump(MidiProtocol::Sender::VST, patchState_.getPatch());
    std::cout << "[VST] Patch dump size=" << sysex.size() << std::endl;
    auto msg = juce::MidiMessage::createSysExMessage(sysex.data() + 1, static_cast<int>(sysex.size()) - 2);
    pendingMidiOut_.addEvent(msg, 0);
}

void DaisyMultiFXProcessor::sendEffectMeta()
{
    // Rate limit responses
    double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastEffectMetaTime_ < kMinResponseIntervalMs)
    {
        std::cout << "[VST] sendEffectMeta rate-limited" << std::endl;
        return;
    }
    lastEffectMetaTime_ = now;

    std::cout << "[VST] Sending EFFECT_META response" << std::endl;
    // Effect metadata (same as before)
    std::vector<uint8_t> sysex;
    sysex.push_back(0xF0);
    sysex.push_back(MidiProtocol::MANUFACTURER_ID);
    sysex.push_back(MidiProtocol::Sender::VST);
    sysex.push_back(MidiProtocol::Resp::EFFECT_META);

    struct EffectInfo
    {
        uint8_t typeId;
        const char *name;
        std::vector<std::pair<uint8_t, const char *>> params;
    };

    std::vector<EffectInfo> effects = {
        {0, "Off", {}},
        {1, "Delay", {{0, "Time"}, {1, "Division"}, {2, "Sync"}, {3, "Feedback"}, {4, "Mix"}}},
        {10, "Distortion", {{0, "Drive"}, {1, "Tone"}}},
        {12, "Sweep Delay", {{0, "Time"}, {1, "Division"}, {2, "Sync"}, {3, "Feedback"}, {4, "Mix"}, {5, "Pan Depth"}, {6, "Pan Rate"}}},
        {13, "Mixer", {{0, "Mix A"}, {1, "Mix B"}, {2, "Cross"}}},
        {14, "Reverb", {{0, "Mix"}, {1, "Decay"}, {2, "Damp"}, {3, "Pre-delay"}, {4, "Size"}}},
        {15, "Compressor", {{0, "Threshold"}, {1, "Ratio"}, {2, "Attack"}, {3, "Release"}, {4, "Makeup"}}},
        {16, "Chorus", {{0, "Rate"}, {1, "Depth"}, {2, "Feedback"}, {3, "Delay"}, {4, "Mix"}}}};

    sysex.push_back(static_cast<uint8_t>(effects.size()));

    for (const auto &fx : effects)
    {
        sysex.push_back(fx.typeId);
        size_t nameLen = strlen(fx.name);
        sysex.push_back(static_cast<uint8_t>(nameLen));
        for (size_t c = 0; c < nameLen; ++c)
            sysex.push_back(static_cast<uint8_t>(fx.name[c]) & 0x7F);
        sysex.push_back(static_cast<uint8_t>(fx.params.size()));
        for (const auto &param : fx.params)
        {
            sysex.push_back(param.first);
            size_t pnameLen = strlen(param.second);
            sysex.push_back(static_cast<uint8_t>(pnameLen));
            for (size_t c = 0; c < pnameLen; ++c)
                sysex.push_back(static_cast<uint8_t>(param.second[c]) & 0x7F);
        }
    }

    sysex.push_back(0xF7);
    auto msg = juce::MidiMessage::createSysExMessage(sysex.data() + 1, static_cast<int>(sysex.size()) - 2);
    pendingMidiOut_.addEvent(msg, 0);
}

//=============================================================================
// Audio Processing
//=============================================================================

void DaisyMultiFXProcessor::prepareToPlay(double sampleRate, int)
{
    processor_->Init(static_cast<float>(sampleRate));
    processor_->ApplyPatch(patchState_.getPatch());
    isPrepared_ = true;
}

void DaisyMultiFXProcessor::releaseResources()
{
    isPrepared_ = false;
}

bool DaisyMultiFXProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DaisyMultiFXProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (!isPrepared_ || !processor_)
        return;

    // Get tempo from host
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

    // Handle incoming MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        handleIncomingMidi(msg);
    }

    // CRITICAL: Clear input messages before adding output
    // Otherwise IAC Driver loops back both input AND output, causing infinite loop
    midiMessages.clear();

    // Send pending MIDI output
    if (!pendingMidiOut_.isEmpty())
    {
        midiMessages.addEvents(pendingMidiOut_, 0, buffer.getNumSamples(), 0);
        pendingMidiOut_.clear();
    }

    // Process audio
    const int numChannels = buffer.getNumChannels();
    auto *leftIn = buffer.getReadPointer(0);
    auto *rightIn = (numChannels > 1) ? buffer.getReadPointer(1) : leftIn;
    auto *leftOut = buffer.getWritePointer(0);
    auto *rightOut = (numChannels > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    float maxIn = 0.0f, maxOut = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inL = leftIn[i], inR = rightIn[i];
        maxIn = std::max(maxIn, std::max(std::abs(inL), std::abs(inR)));

        float outL, outR;
        processor_->ProcessFrame(inL, inR, outL, outR);
        leftOut[i] = outL;
        rightOut[i] = outR;

        maxOut = std::max(maxOut, std::max(std::abs(outL), std::abs(outR)));
    }

    // Update level meters
    const float release = 0.97f;
    float currentIn = inputLevel_.load();
    inputLevel_.store(maxIn > currentIn ? maxIn : currentIn * release);
    float currentOut = outputLevel_.load();
    outputLevel_.store(maxOut > currentOut ? maxOut : currentOut * release);
}

//=============================================================================
// Plugin Metadata
//=============================================================================

juce::AudioProcessorEditor *DaisyMultiFXProcessor::createEditor()
{
    return new DaisyMultiFXEditor(*this);
}

bool DaisyMultiFXProcessor::hasEditor() const { return true; }
const juce::String DaisyMultiFXProcessor::getName() const { return JucePlugin_Name; }
bool DaisyMultiFXProcessor::acceptsMidi() const { return true; }
bool DaisyMultiFXProcessor::producesMidi() const { return true; }
bool DaisyMultiFXProcessor::isMidiEffect() const { return false; }
double DaisyMultiFXProcessor::getTailLengthSeconds() const { return 2.0; }
int DaisyMultiFXProcessor::getNumPrograms() { return 1; }
int DaisyMultiFXProcessor::getCurrentProgram() { return 0; }
void DaisyMultiFXProcessor::setCurrentProgram(int) {}
const juce::String DaisyMultiFXProcessor::getProgramName(int) { return {}; }
void DaisyMultiFXProcessor::changeProgramName(int, const juce::String &) {}

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
        parameters_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DaisyMultiFXProcessor();
}
