#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/effects/effect_metadata.h"
#include "core/protocol/sysex_protocol.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <fstream>

// For JSON model loading with Neural Amp
#if __has_include(<RTNeural/RTNeural.h>)
#define HAS_RTNEURAL 1
// Use RTNeural's bundled json (included via model_loader.h)
#include <RTNeural/model_loader.h>
#else
#define HAS_RTNEURAL 0
#endif

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
        for (int p = 0; p < kMaxParamsPerSlot; ++p)
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
            Effects::getIndexByTypeId(def.typeId))); // Compute index from typeId at runtime

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "_mix", 1),
            "Slot " + juce::String(slot + 1) + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        for (int p = 0; p < kMaxParamsPerSlot; ++p)
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
    // Use shared default patch from core
    PatchWireDesc patch = daisyfx::MakeDefaultPatch();

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
        for (int p = 0; p < kMaxParamsPerSlot; ++p)
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

void DaisyMultiFXProcessor::onSlotRoutingChanged(uint8_t slot, uint8_t inputL, uint8_t inputR)
{
    // Update DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());
}

void DaisyMultiFXProcessor::onSlotSumToMonoChanged(uint8_t slot, bool sumToMono)
{
    // Update DSP
    if (isPrepared_)
        processor_->ApplyPatch(patchState_.getPatch());
}

void DaisyMultiFXProcessor::onSlotChannelPolicyChanged(uint8_t slot, uint8_t channelPolicy)
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

void DaisyMultiFXProcessor::onInputGainChanged(float gainDb)
{
    // Convert dB to linear gain: gain = 10^(dB/20)
    float linearGain = std::pow(10.0f, gainDb / 20.0f);
    processor_->SetInputGain(linearGain);
}

void DaisyMultiFXProcessor::onOutputGainChanged(float gainDb)
{
    // Convert dB to linear gain: gain = 10^(dB/20)
    float linearGain = std::pow(10.0f, gainDb / 20.0f);
    processor_->SetOutputGain(linearGain);
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

    case MidiProtocol::Cmd::SET_ROUTING:
        patchState_.setSlotRouting(decoded.slot, decoded.inputL, decoded.inputR);
        break;

    case MidiProtocol::Cmd::SET_SUM_TO_MONO:
        patchState_.setSlotSumToMono(decoded.slot, decoded.sumToMono);
        break;

    case MidiProtocol::Cmd::SET_MIX:
        // Protocol payload order is <dry> <wet>, but PatchState API is (wet, dry)
        patchState_.setSlotMix(decoded.slot, decoded.wet, decoded.dry);
        break;

    case MidiProtocol::Cmd::SET_CHANNEL_POLICY:
        patchState_.setSlotChannelPolicy(decoded.slot, decoded.channelPolicy);
        break;

    case MidiProtocol::Cmd::REQUEST_PATCH:
        sendPatchDump();
        break;

    case MidiProtocol::Cmd::REQUEST_META:
        sendEffectMeta();
        break;

    case MidiProtocol::Cmd::SET_INPUT_GAIN:
        patchState_.setInputGainDb(decoded.inputGainDb);
        break;

    case MidiProtocol::Cmd::SET_OUTPUT_GAIN:
        patchState_.setOutputGainDb(decoded.outputGainDb);
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

    // Also send V3 metadata per effect (includes 3-character shortName).
    // Format: F0 7D <sender> 36 <typeId> <nameLen> <name...> <shortName[3]> <numParams>
    //         (paramId kind nameLen name...)xN F7
    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        const auto &entry = Effects::kAllEffects[i];
        const auto *meta = entry.meta;

        std::vector<uint8_t> v3;
        v3.reserve(256);
        v3.push_back(0xF0);
        v3.push_back(MidiProtocol::MANUFACTURER_ID);
        v3.push_back(MidiProtocol::Sender::VST);
        v3.push_back(MidiProtocol::Resp::EFFECT_META_V3);

        v3.push_back(static_cast<uint8_t>(entry.typeId) & 0x7F);

        const char *name = meta && meta->name ? meta->name : "";
        size_t nameLen = std::strlen(name);
        if (nameLen > 60)
            nameLen = 60;
        v3.push_back(static_cast<uint8_t>(nameLen) & 0x7F);
        for (size_t c = 0; c < nameLen; ++c)
            v3.push_back(static_cast<uint8_t>(name[c]) & 0x7F);

        const char *shortName = (meta && meta->shortName) ? meta->shortName : "---";
        for (int k = 0; k < 3; ++k)
            v3.push_back(static_cast<uint8_t>(shortName[k]) & 0x7F);

        const uint8_t numParams = meta ? meta->numParams : 0;
        v3.push_back(numParams & 0x7F);
        for (uint8_t p = 0; p < numParams; ++p)
        {
            const auto &param = meta->params[p];
            v3.push_back(static_cast<uint8_t>(param.id) & 0x7F);
            v3.push_back(static_cast<uint8_t>(param.kind) & 0x7F);

            const char *pname = param.name ? param.name : "";
            size_t pnameLen = std::strlen(pname);
            if (pnameLen > 24)
                pnameLen = 24;
            v3.push_back(static_cast<uint8_t>(pnameLen) & 0x7F);
            for (size_t c = 0; c < pnameLen; ++c)
                v3.push_back(static_cast<uint8_t>(pname[c]) & 0x7F);
        }

        v3.push_back(0xF7);
        auto v3Msg = juce::MidiMessage::createSysExMessage(v3.data() + 1, static_cast<int>(v3.size()) - 2);
        pendingMidiOut_.addEvent(v3Msg, 0);
    }

    // Also send V4 metadata per effect (adds number ranges encoded as Q16.16).
    // Format: F0 7D <sender> 37 <typeId> <nameLen> <name...> <shortName[3]> <numParams>
    //         (paramId kind flags nameLen name... [min] [max] [step])xN F7
    // where min/max/step are Q16.16 packed into 5 bytes each (7-bit safe).
    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        const auto &entry = Effects::kAllEffects[i];
        const auto *meta = entry.meta;

        std::vector<uint8_t> v4;
        v4.reserve(320);
        v4.push_back(0xF0);
        v4.push_back(MidiProtocol::MANUFACTURER_ID);
        v4.push_back(MidiProtocol::Sender::VST);
        v4.push_back(MidiProtocol::Resp::EFFECT_META_V4);

        v4.push_back(static_cast<uint8_t>(entry.typeId) & 0x7F);

        const char *name = meta && meta->name ? meta->name : "";
        size_t nameLen = std::strlen(name);
        if (nameLen > 60)
            nameLen = 60;
        v4.push_back(static_cast<uint8_t>(nameLen) & 0x7F);
        for (size_t c = 0; c < nameLen; ++c)
            v4.push_back(static_cast<uint8_t>(name[c]) & 0x7F);

        const char *shortName = (meta && meta->shortName) ? meta->shortName : "---";
        for (int k = 0; k < 3; ++k)
            v4.push_back(static_cast<uint8_t>(shortName[k]) & 0x7F);

        const uint8_t numParams = meta ? meta->numParams : 0;
        v4.push_back(numParams & 0x7F);

        for (uint8_t p = 0; p < numParams; ++p)
        {
            const auto &param = meta->params[p];
            v4.push_back(static_cast<uint8_t>(param.id) & 0x7F);
            v4.push_back(static_cast<uint8_t>(param.kind) & 0x7F);

            uint8_t flags = 0;
            const bool hasNumberRange = (param.kind == ParamValueKind::Number) && (param.number != nullptr);
            if (hasNumberRange)
                flags |= 0x01;
            v4.push_back(flags & 0x7F);

            const char *pname = param.name ? param.name : "";
            size_t pnameLen = std::strlen(pname);
            if (pnameLen > 24)
                pnameLen = 24;
            v4.push_back(static_cast<uint8_t>(pnameLen) & 0x7F);
            for (size_t c = 0; c < pnameLen; ++c)
                v4.push_back(static_cast<uint8_t>(pname[c]) & 0x7F);

            if (hasNumberRange)
            {
                uint8_t packed[5];

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(param.number->minValue), packed);
                v4.insert(v4.end(), packed, packed + 5);

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(param.number->maxValue), packed);
                v4.insert(v4.end(), packed, packed + 5);

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(param.number->step), packed);
                v4.insert(v4.end(), packed, packed + 5);
            }
        }

        v4.push_back(0xF7);
        auto v4Msg = juce::MidiMessage::createSysExMessage(v4.data() + 1, static_cast<int>(v4.size()) - 2);
        pendingMidiOut_.addEvent(v4Msg, 0);
    }

    // Also send V5 metadata per effect (adds effect/param descriptions + unit prefix/suffix).
    // Format: F0 7D <sender> 38 <typeId> <nameLen> <name...> <shortName[3]> <effectDescLen> <effectDesc...> <numParams>
    //         (paramId kind flags nameLen name... descLen desc... unitPreLen unitPre... unitSufLen unitSuf...
    //          [min] [max] [step])xN F7
    auto inferUnitSuffix = [](uint8_t effectTypeId, const ParamInfo &par) -> const char *
    {
        // 1) Parse from name: "Foo (ms)" etc
        if (par.name)
        {
            const char *open = std::strchr(par.name, '(');
            const char *close = open ? std::strchr(open, ')') : nullptr;
            if (open && close && close > open + 1)
                return open + 1;
        }

        // 2) Parse from description: "Attack time (ms)" etc
        if (par.description)
        {
            const char *open = std::strchr(par.description, '(');
            const char *close = open ? std::strchr(open, ')') : nullptr;
            if (open && close && close > open + 1)
                return open + 1;
        }

        // 3) Known special-cases
        if (effectTypeId == Effects::GraphicEQ::TypeId)
            return "dB";

        return "";
    };

    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        const auto &entry = Effects::kAllEffects[i];
        const auto *meta = entry.meta;

        std::vector<uint8_t> v5;
        v5.reserve(420);
        v5.push_back(0xF0);
        v5.push_back(MidiProtocol::MANUFACTURER_ID);
        v5.push_back(MidiProtocol::Sender::VST);
        v5.push_back(MidiProtocol::Resp::EFFECT_META_V5);

        v5.push_back(static_cast<uint8_t>(entry.typeId) & 0x7F);

        const char *name = meta && meta->name ? meta->name : "";
        size_t nameLen = std::strlen(name);
        if (nameLen > 60)
            nameLen = 60;
        v5.push_back(static_cast<uint8_t>(nameLen) & 0x7F);
        for (size_t c = 0; c < nameLen; ++c)
            v5.push_back(static_cast<uint8_t>(name[c]) & 0x7F);

        const char *shortName = (meta && meta->shortName) ? meta->shortName : "---";
        for (int k = 0; k < 3; ++k)
            v5.push_back(static_cast<uint8_t>(shortName[k]) & 0x7F);

        const char *effectDesc = (meta && meta->description) ? meta->description : "";
        size_t effectDescLen = std::strlen(effectDesc);
        if (effectDescLen > 80)
            effectDescLen = 80;
        v5.push_back(static_cast<uint8_t>(effectDescLen) & 0x7F);
        for (size_t c = 0; c < effectDescLen; ++c)
            v5.push_back(static_cast<uint8_t>(effectDesc[c]) & 0x7F);

        const uint8_t numParams = meta ? meta->numParams : 0;
        v5.push_back(numParams & 0x7F);

        for (uint8_t p = 0; p < numParams; ++p)
        {
            const auto &par = meta->params[p];
            v5.push_back(static_cast<uint8_t>(par.id) & 0x7F);
            v5.push_back(static_cast<uint8_t>(par.kind) & 0x7F);

            uint8_t flags = 0;
            const bool hasNumberRange = (par.kind == ParamValueKind::Number) && (par.number != nullptr);
            const bool hasEnumOptions = (par.kind == ParamValueKind::Enum) && (par.enumeration != nullptr) && (par.enumeration->numOptions > 0);
            if (hasNumberRange)
                flags |= 0x01;
            if (hasEnumOptions)
                flags |= 0x02;
            v5.push_back(flags & 0x7F);

            const char *pname = par.name ? par.name : "";
            size_t pnameLen = std::strlen(pname);
            if (pnameLen > 24)
                pnameLen = 24;
            v5.push_back(static_cast<uint8_t>(pnameLen) & 0x7F);
            for (size_t c = 0; c < pnameLen; ++c)
                v5.push_back(static_cast<uint8_t>(pname[c]) & 0x7F);

            const char *pdesc = par.description ? par.description : "";
            size_t pdescLen = std::strlen(pdesc);
            if (pdescLen > 60)
                pdescLen = 60;
            v5.push_back(static_cast<uint8_t>(pdescLen) & 0x7F);
            for (size_t c = 0; c < pdescLen; ++c)
                v5.push_back(static_cast<uint8_t>(pdesc[c]) & 0x7F);

            // Unit prefix (kept empty for now)
            v5.push_back(0);

            // Unit suffix (best-effort inference)
            const char *unitSuf = inferUnitSuffix(static_cast<uint8_t>(entry.typeId), par);
            // inferUnitSuffix returns pointer inside original string; cap at ')'
            size_t unitSufLen = 0;
            if (unitSuf && unitSuf[0] != '\0')
            {
                const char *close = std::strchr(unitSuf, ')');
                unitSufLen = close ? static_cast<size_t>(close - unitSuf) : std::strlen(unitSuf);
                if (unitSufLen > 8)
                    unitSufLen = 8;

                // Reject if it looks like a phrase (spaces), not a unit.
                if (std::memchr(unitSuf, ' ', unitSufLen) != nullptr)
                    unitSufLen = 0;
            }
            v5.push_back(static_cast<uint8_t>(unitSufLen) & 0x7F);
            for (size_t c = 0; c < unitSufLen; ++c)
                v5.push_back(static_cast<uint8_t>(unitSuf[c]) & 0x7F);

            if (hasNumberRange)
            {
                uint8_t packed[5];

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(par.number->minValue), packed);
                v5.insert(v5.end(), packed, packed + 5);

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(par.number->maxValue), packed);
                v5.insert(v5.end(), packed, packed + 5);

                DaisyMultiFX::Protocol::packQ16_16(
                    DaisyMultiFX::Protocol::floatToQ16_16(par.number->step), packed);
                v5.insert(v5.end(), packed, packed + 5);
            }

            // Enum options (if Enum kind)
            if (hasEnumOptions)
            {
                const uint8_t numOpts = par.enumeration->numOptions;
                v5.push_back(numOpts & 0x7F);
                for (uint8_t oi = 0; oi < numOpts; oi++)
                {
                    const auto &opt = par.enumeration->options[oi];
                    v5.push_back(opt.value & 0x7F);
                    const char *optName = opt.name ? opt.name : "";
                    size_t optNameLen = std::strlen(optName);
                    if (optNameLen > 24)
                        optNameLen = 24;
                    v5.push_back(static_cast<uint8_t>(optNameLen) & 0x7F);
                    for (size_t c = 0; c < optNameLen; ++c)
                        v5.push_back(static_cast<uint8_t>(optName[c]) & 0x7F);
                }
            }
        }

        v5.push_back(0xF7);
        auto v5Msg = juce::MidiMessage::createSysExMessage(v5.data() + 1, static_cast<int>(v5.size()) - 2);
        pendingMidiOut_.addEvent(v5Msg, 0);
    }
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

//=============================================================================
// Neural Amp Model Loading
//=============================================================================

bool DaisyMultiFXProcessor::loadNeuralAmpModel(int slotIndex, const juce::File &modelFile)
{
#if HAS_RTNEURAL
    if (!modelFile.existsAsFile())
        return false;

    // Find which Neural Amp instance is in this slot
    // Neural amps share the same pool, so we need to find the right one
    const auto &patch = patchState_.getPatch();
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return false;

    // Check if this slot has Neural Amp effect
    if (patch.slots[slotIndex].typeId != Effects::NeuralAmp::TypeId)
        return false;

    // Count how many Neural Amp slots come before this one to find the pool index
    int neuralAmpIndex = 0;
    for (int i = 0; i < slotIndex; ++i)
    {
        if (patch.slots[i].typeId == Effects::NeuralAmp::TypeId)
            ++neuralAmpIndex;
    }

    // Get the Neural Amp effect from the pool
    NeuralAmpEffect *neuralAmp = processor_->GetNeuralAmpEffect(neuralAmpIndex);
    if (!neuralAmp)
        return false;

    try
    {
        // Read and parse the JSON file
        std::ifstream file(modelFile.getFullPathName().toStdString());
        if (!file.is_open())
            return false;

        nlohmann::json modelJson;
        file >> modelJson;

        // Extract model name from filename (without extension)
        std::string modelName = modelFile.getFileNameWithoutExtension().toStdString();
        std::string modelPath = modelFile.getFullPathName().toStdString();

        // Load the model
        bool success = neuralAmp->LoadModelFromJson(modelJson, modelName, modelPath);

        return success;
    }
    catch (...)
    {
        return false;
    }
#else
    juce::ignoreUnused(slotIndex, modelFile);
    return false;
#endif
}

juce::String DaisyMultiFXProcessor::getNeuralAmpModelName(int slotIndex) const
{
#if HAS_RTNEURAL
    const auto &patch = patchState_.getPatch();
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return "Invalid Slot";

    // Check if this slot has Neural Amp effect
    if (patch.slots[slotIndex].typeId != Effects::NeuralAmp::TypeId)
        return "Not Neural Amp";

    // Count how many Neural Amp slots come before this one
    int neuralAmpIndex = 0;
    for (int i = 0; i < slotIndex; ++i)
    {
        if (patch.slots[i].typeId == Effects::NeuralAmp::TypeId)
            ++neuralAmpIndex;
    }

    // Get the Neural Amp effect - note: processor_ is non-const, so we need const_cast
    NeuralAmpEffect *neuralAmp = const_cast<DaisyMultiFXProcessor *>(this)->processor_->GetNeuralAmpEffect(neuralAmpIndex);
    if (!neuralAmp)
        return "No Effect";

    return juce::String(neuralAmp->GetModelName());
#else
    juce::ignoreUnused(slotIndex);
    return "RTNeural Disabled";
#endif
}

//=============================================================================
// Cabinet IR Loading
//=============================================================================

bool DaisyMultiFXProcessor::loadCabinetIR(int slotIndex, const juce::File &irFile)
{
    if (!irFile.existsAsFile())
        return false;

    const auto &patch = patchState_.getPatch();
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return false;

    // Check if this slot has Cabinet IR effect
    if (patch.slots[slotIndex].typeId != Effects::CabinetIR::TypeId)
        return false;

    // Count how many Cabinet IR slots come before this one to find the pool index
    int cabinetIRIndex = 0;
    for (int i = 0; i < slotIndex; ++i)
    {
        if (patch.slots[i].typeId == Effects::CabinetIR::TypeId)
            ++cabinetIRIndex;
    }

    // Get the Cabinet IR effect from the pool
    CabinetIREffect *cabinetIR = processor_->GetCabinetIREffect(cabinetIRIndex);
    if (!cabinetIR)
        return false;

    // Use JUCE to read the WAV file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(irFile));
    if (!reader)
        return false;

    // Read the IR samples - we'll use mono (or sum to mono if stereo)
    int numSamples = static_cast<int>(reader->lengthInSamples);
    if (numSamples <= 0)
        return false;

    // Limit to max IR length
    if (numSamples > CabinetIREffect::kMaxIRLength)
        numSamples = CabinetIREffect::kMaxIRLength;

    // Read into a temporary buffer
    juce::AudioBuffer<float> tempBuffer(1, numSamples);

    if (reader->numChannels == 1)
    {
        // Mono file - read directly
        reader->read(&tempBuffer, 0, numSamples, 0, true, false);
    }
    else
    {
        // Stereo or more - read both channels and average
        juce::AudioBuffer<float> stereoBuffer(static_cast<int>(reader->numChannels), numSamples);
        reader->read(&stereoBuffer, 0, numSamples, 0, true, true);

        // Sum to mono
        tempBuffer.clear();
        for (int ch = 0; ch < static_cast<int>(reader->numChannels); ++ch)
        {
            tempBuffer.addFrom(0, 0, stereoBuffer, ch, 0, numSamples, 1.0f / reader->numChannels);
        }
    }

    // Extract model name from filename (without extension)
    std::string irName = irFile.getFileNameWithoutExtension().toStdString();
    std::string irPath = irFile.getFullPathName().toStdString();

    // Load the IR into the effect
    return cabinetIR->LoadIR(tempBuffer.getReadPointer(0), numSamples, irName, irPath);
}

juce::String DaisyMultiFXProcessor::getCabinetIRName(int slotIndex) const
{
    const auto &patch = patchState_.getPatch();
    if (slotIndex < 0 || slotIndex >= kNumSlots)
        return "Invalid Slot";

    // Check if this slot has Cabinet IR effect
    if (patch.slots[slotIndex].typeId != Effects::CabinetIR::TypeId)
        return "Not Cabinet IR";

    // Count how many Cabinet IR slots come before this one
    int cabinetIRIndex = 0;
    for (int i = 0; i < slotIndex; ++i)
    {
        if (patch.slots[i].typeId == Effects::CabinetIR::TypeId)
            ++cabinetIRIndex;
    }

    // Get the Cabinet IR effect
    CabinetIREffect *cabinetIR = const_cast<DaisyMultiFXProcessor *>(this)->processor_->GetCabinetIREffect(cabinetIRIndex);
    if (!cabinetIR)
        return "No Effect";

    return juce::String(cabinetIR->GetIRName());
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DaisyMultiFXProcessor();
}
