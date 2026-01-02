#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/effects/effect_metadata.h"
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
    for (int slot = 0; slot < kNumSlots; ++slot)
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

    // Build effect type choices from shared metadata
    juce::StringArray effectNames;
    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        effectNames.add(Effects::kAllEffects[i].meta->name);
    }

    // Per-slot parameters - use kDefaultSlots for proper defaults
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);
        const auto &def = kDefaultSlots[slot];

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "_enabled", 1),
            "Slot " + juce::String(slot + 1) + " Enabled", true));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "_type", 1),
            "Slot " + juce::String(slot + 1) + " Type",
            effectNames,
            def.typeIndex)); // Use default effect type index

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "_mix", 1),
            "Slot " + juce::String(slot + 1) + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        for (int p = 0; p < 5; ++p)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(prefix + "_p" + juce::String(p), 1),
                "Slot " + juce::String(slot + 1) + " Param " + juce::String(p + 1),
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), def.params[p]));
        }
    }

    return {params.begin(), params.end()};
}

void DaisyMultiFXProcessor::initializeDefaultPatch()
{
    // Create default patch from kDefaultSlots configuration
    PatchWireDesc patch = {};
    patch.numSlots = kNumSlots;

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        const auto &def = kDefaultSlots[slot];
        patch.slots[slot].slotIndex = static_cast<uint8_t>(slot);
        patch.slots[slot].typeId = def.typeId;
        patch.slots[slot].enabled = 1;
        patch.slots[slot].inputL = (slot == 0) ? ROUTE_INPUT : static_cast<uint8_t>(slot - 1);
        patch.slots[slot].inputR = (slot == 0) ? ROUTE_INPUT : static_cast<uint8_t>(slot - 1);
        patch.slots[slot].sumToMono = (slot == 0) ? 1 : 0;
        patch.slots[slot].wet = 127;
        patch.slots[slot].numParams = 5;
        for (int p = 0; p < 5; ++p)
        {
            patch.slots[slot].params[p] = {static_cast<uint8_t>(p),
                                           static_cast<uint8_t>(def.params[p] * 127.0f)};
        }
    }

    // Load into PatchState (will notify observers)
    patchState_.loadPatch(patch);
    tempo_.bpm = 120.0f;
    tempo_.valid = true;
}

void DaisyMultiFXProcessor::syncParametersFromPatch()
{
    isUpdatingFromPatchState_ = true;

    const auto &patch = patchState_.getPatch();

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);
        const auto &slotData = patch.slots[slot];

        *parameters_.getRawParameterValue(prefix + "_type") = static_cast<float>(Effects::getIndexByTypeId(slotData.typeId));
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

void DaisyMultiFXProcessor::syncPatchFromParameters()
{
    // Sync APVTS -> PatchState after state restoration
    // This is needed because replaceState() doesn't trigger parameterChanged callbacks
    isUpdatingFromAPVTS_ = true;

    // Update tempo
    float tempo = *parameters_.getRawParameterValue("tempo");
    patchState_.setTempo(tempo);

    // Update each slot
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        juce::String prefix = "slot" + juce::String(slot);

        // Type - read from choice parameter
        float typeIndexFloat = *parameters_.getRawParameterValue(prefix + "_type");
        int typeIndex = static_cast<int>(typeIndexFloat + 0.5f);
        if (typeIndex >= 0 && typeIndex < static_cast<int>(Effects::kNumEffects))
            patchState_.setSlotType(static_cast<uint8_t>(slot), Effects::getTypeIdByIndex(static_cast<size_t>(typeIndex)));

        // Enabled
        float enabled = *parameters_.getRawParameterValue(prefix + "_enabled");
        patchState_.setSlotEnabled(static_cast<uint8_t>(slot), enabled > 0.5f);

        // Mix
        float mix = *parameters_.getRawParameterValue(prefix + "_mix");
        patchState_.setSlotMix(static_cast<uint8_t>(slot), static_cast<uint8_t>(mix * 127.0f), 0);

        // Parameters
        for (int p = 0; p < 5; ++p)
        {
            float paramValue = *parameters_.getRawParameterValue(prefix + "_p" + juce::String(p));
            patchState_.setSlotParam(static_cast<uint8_t>(slot), static_cast<uint8_t>(p), static_cast<uint8_t>(paramValue * 127.0f));
        }
    }

    isUpdatingFromAPVTS_ = false;

    // Now apply the patch to DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());
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
        if (slotIndex < 0 || slotIndex >= kNumSlots)
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
            int typeIndex = static_cast<int>(newValue);
            if (typeIndex >= 0 && typeIndex < static_cast<int>(Effects::kNumEffects))
                patchState_.setSlotType(static_cast<uint8_t>(slotIndex), Effects::getTypeIdByIndex(static_cast<size_t>(typeIndex)));
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
    if (!isUpdatingFromPatchState_ && slot < kNumSlots)
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
    if (!isUpdatingFromPatchState_ && slot < kNumSlots)
    {
        isUpdatingFromPatchState_ = true;
        if (auto *param = dynamic_cast<juce::AudioParameterChoice *>(
                parameters_.getParameter("slot" + juce::String(slot) + "_type")))
        {
            float normalizedValue = Effects::getIndexByTypeId(typeId) / static_cast<float>(param->choices.size() - 1);
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
    if (!isUpdatingFromPatchState_ && slot < kNumSlots && paramId < 5)
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
        return;
    }
    lastEffectMetaTime_ = now;

    // Build EFFECT_META response from the shared metadata
    std::vector<uint8_t> sysex;
    sysex.push_back(0xF0);
    sysex.push_back(MidiProtocol::MANUFACTURER_ID);
    sysex.push_back(MidiProtocol::Sender::VST);
    sysex.push_back(MidiProtocol::Resp::EFFECT_META);

    sysex.push_back(static_cast<uint8_t>(Effects::kNumEffects));

    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        const auto &entry = Effects::kAllEffects[i];
        const auto *meta = entry.meta;

        sysex.push_back(entry.typeId);
        size_t nameLen = strlen(meta->name);
        sysex.push_back(static_cast<uint8_t>(nameLen));
        for (size_t c = 0; c < nameLen; ++c)
            sysex.push_back(static_cast<uint8_t>(meta->name[c]) & 0x7F);
        sysex.push_back(meta->numParams);
        for (uint8_t p = 0; p < meta->numParams; ++p)
        {
            const auto &param = meta->params[p];
            sysex.push_back(param.id);
            size_t pnameLen = strlen(param.name);
            sysex.push_back(static_cast<uint8_t>(pnameLen));
            for (size_t c = 0; c < pnameLen; ++c)
                sysex.push_back(static_cast<uint8_t>(param.name[c]) & 0x7F);
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
    const int numSamples = buffer.getNumSamples();
    auto *leftIn = buffer.getReadPointer(0);
    auto *rightIn = (numChannels > 1) ? buffer.getReadPointer(1) : leftIn;
    auto *leftOut = buffer.getWritePointer(0);
    auto *rightOut = (numChannels > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    // Detect mono input: check if right channel is essentially silent while left has signal
    // This handles the common case of a mono guitar input on a stereo bus
    bool monoInput = false;
    if (numChannels > 1)
    {
        float leftEnergy = 0.0f, rightEnergy = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            leftEnergy += leftIn[i] * leftIn[i];
            rightEnergy += rightIn[i] * rightIn[i];
        }
        // If left has significant signal but right is near-silent, treat as mono
        monoInput = (leftEnergy > 1e-8f && rightEnergy < leftEnergy * 0.001f);
    }

    float maxIn = 0.0f, maxOut = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = leftIn[i];
        float inR = monoInput ? inL : rightIn[i]; // Copy L to R for mono input

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
    {
        parameters_.replaceState(juce::ValueTree::fromXml(*xml));
        // Sync restored parameters to PatchState and DSP
        syncPatchFromParameters();
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DaisyMultiFXProcessor();
}
