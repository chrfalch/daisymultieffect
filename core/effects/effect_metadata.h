#pragma once

#include "base_effect.h"
#include <cstdint>

/**
 * Effect Metadata - Single source of truth for all effect metadata.
 *
 * This file defines the static metadata (names, parameters, ranges) for all effects.
 * It uses the EffectMeta/ParamInfo structs from base_effect.h.
 *
 * Used by:
 * - VST: APVTS parameter layout, editor labels, MIDI responses
 * - Firmware: Effect classes reference these via GetMetadata()
 * - Swift/Host apps: Receive via MIDI EFFECT_META response
 *
 * To add a new effect:
 * 1. Add a new namespace below with TypeId, param ranges, params array, and kMeta
 * 2. Add to kAllEffects array at the bottom
 * 3. Implement the effect class in firmware/src/effects/
 */

namespace Effects
{

    //=========================================================================
    // Off (bypass)
    //=========================================================================
    namespace Off
    {
        constexpr uint8_t TypeId = 0;
        inline const ::EffectMeta kMeta = {"Off", "OFF", "Bypass/disabled", nullptr, 0};
    }

    //=========================================================================
    // Delay
    //=========================================================================
    namespace Delay
    {
        constexpr uint8_t TypeId = 1;
        inline const NumberParamRange kFbRange = {0.0f, 0.95f, 0.01f};
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Free Time", "Delay time ms if not synced", ParamValueKind::Number, nullptr, nullptr},
            {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
            {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
            {3, "Feedback", "Delay feedback", ParamValueKind::Number, &kFbRange, nullptr},
            {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Delay", "DLY", "Tempo-synced delay.", kParams, 5};
    }

    //=========================================================================
    // Distortion
    //=========================================================================
    namespace Distortion
    {
        constexpr uint8_t TypeId = 10;
        inline const NumberParamRange kDriveRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kToneRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Drive", "Overdrive amount", ParamValueKind::Number, &kDriveRange, nullptr},
            {1, "Tone", "Dark to bright", ParamValueKind::Number, &kToneRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Overdrive", "OVD", "Musical overdrive with auto-leveling.", kParams, 2};
    }

    //=========================================================================
    // Sweep Delay
    //=========================================================================
    namespace SweepDelay
    {
        constexpr uint8_t TypeId = 12;
        inline const NumberParamRange kFbRange = {0.0f, 0.95f, 0.01f};
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kPanDepthRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kPanRateRange = {0.05f, 5.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Free Time", "Delay time if not synced", ParamValueKind::Number, nullptr, nullptr},
            {1, "Division", "Beat division index", ParamValueKind::Number, nullptr, nullptr},
            {2, "Synced", "0/1 tempo synced", ParamValueKind::Number, nullptr, nullptr},
            {3, "Feedback", "Feedback", ParamValueKind::Number, &kFbRange, nullptr},
            {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
            {5, "Pan Depth", "Pan sweep depth", ParamValueKind::Number, &kPanDepthRange, nullptr},
            {6, "Pan Rate", "Pan rate (Hz)", ParamValueKind::Number, &kPanRateRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Sweep Delay", "SWP", "Stereo delay with pan sweep.", kParams, 7};
    }

    //=========================================================================
    // Stereo Mixer
    //=========================================================================
    namespace Mixer
    {
        constexpr uint8_t TypeId = 13;
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kCrossRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Mix A", "Level for branch A", ParamValueKind::Number, &kMixRange, nullptr},
            {1, "Mix B", "Level for branch B", ParamValueKind::Number, &kMixRange, nullptr},
            {2, "Cross", "Cross-couple A/B", ParamValueKind::Number, &kCrossRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Mixer", "MIX", "Mix two branches into stereo.", kParams, 3};
    }

    //=========================================================================
    // Reverb
    //=========================================================================
    namespace Reverb
    {
        constexpr uint8_t TypeId = 14;
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kDecayRange = {0.2f, 0.95f, 0.01f};
        inline const NumberParamRange kDampRange = {0.0f, 0.8f, 0.01f};
        inline const NumberParamRange kPreRange = {0.0f, 80.0f, 1.0f};
        inline const NumberParamRange kSizeRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
            {1, "Decay", "Reverb decay", ParamValueKind::Number, &kDecayRange, nullptr},
            {2, "Damping", "High damping", ParamValueKind::Number, &kDampRange, nullptr},
            {3, "PreDelay", "Pre-delay (ms)", ParamValueKind::Number, &kPreRange, nullptr},
            {4, "Size", "Room size", ParamValueKind::Number, &kSizeRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Reverb", "REV", "Simple Schroeder reverb.", kParams, 5};
    }

    //=========================================================================
    // Compressor
    //=========================================================================
    namespace Compressor
    {
        constexpr uint8_t TypeId = 15;
        inline const NumberParamRange kThreshRange = {-40.0f, 0.0f, 0.5f};
        inline const NumberParamRange kRatioRange = {1.0f, 20.0f, 0.1f};
        inline const NumberParamRange kAttackRange = {0.1f, 100.0f, 0.1f};
        inline const NumberParamRange kReleaseRange = {10.0f, 1000.0f, 1.0f};
        inline const NumberParamRange kMakeupRange = {0.0f, 24.0f, 0.1f};
        inline const ParamInfo kParams[] = {
            {0, "Threshold", "Threshold (dB)", ParamValueKind::Number, &kThreshRange, nullptr},
            {1, "Ratio", "Compression ratio", ParamValueKind::Number, &kRatioRange, nullptr},
            {2, "Attack", "Attack time (ms)", ParamValueKind::Number, &kAttackRange, nullptr},
            {3, "Release", "Release time (ms)", ParamValueKind::Number, &kReleaseRange, nullptr},
            {4, "Makeup", "Makeup gain (dB)", ParamValueKind::Number, &kMakeupRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Compressor", "CMP", "Dynamics compressor.", kParams, 5};
    }

    //=========================================================================
    // Chorus
    //=========================================================================
    namespace Chorus
    {
        constexpr uint8_t TypeId = 16;
        inline const NumberParamRange kRateRange = {0.1f, 2.0f, 0.01f};
        inline const NumberParamRange kDepthRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kFeedbackRange = {0.0f, 0.9f, 0.01f};
        inline const NumberParamRange kDelayRange = {5.0f, 25.0f, 0.1f};
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Rate", "LFO rate (Hz)", ParamValueKind::Number, &kRateRange, nullptr},
            {1, "Depth", "Modulation depth", ParamValueKind::Number, &kDepthRange, nullptr},
            {2, "Feedback", "Feedback amount", ParamValueKind::Number, &kFeedbackRange, nullptr},
            {3, "Delay", "Base delay (ms)", ParamValueKind::Number, &kDelayRange, nullptr},
            {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Chorus", "CHO", "Classic stereo chorus.", kParams, 5};
    }

    //=========================================================================
    // Noise Gate
    //=========================================================================
    namespace NoiseGate
    {
        constexpr uint8_t TypeId = 17;
        inline const NumberParamRange kThreshRange = {-80.0f, -20.0f, 0.1f};
        inline const NumberParamRange kAttackRange = {0.1f, 50.0f, 0.1f};
        inline const NumberParamRange kHoldRange = {10.0f, 500.0f, 1.0f};
        inline const NumberParamRange kReleaseRange = {10.0f, 500.0f, 1.0f};
        inline const NumberParamRange kRangeRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Threshold", "Gate open level", ParamValueKind::Number, &kThreshRange, nullptr},
            {1, "Attack", "Gate open speed", ParamValueKind::Number, &kAttackRange, nullptr},
            {2, "Hold", "Hold time after signal", ParamValueKind::Number, &kHoldRange, nullptr},
            {3, "Release", "Gate close speed", ParamValueKind::Number, &kReleaseRange, nullptr},
            {4, "Range", "Floor level when closed", ParamValueKind::Number, &kRangeRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Noise Gate", "NGT", "Cut signal below threshold to eliminate hum/buzz.", kParams, 5};
    }

    //=========================================================================
    // Graphic EQ (7-band guitar EQ)
    //=========================================================================
    namespace GraphicEQ
    {
        constexpr uint8_t TypeId = 18;
        // All bands: -12dB to +12dB (0.5 = 0dB flat)
        inline const NumberParamRange kBandRange = {-12.0f, 12.0f, 0.5f};
        inline const ParamInfo kParams[] = {
            {0, "100 Hz", "Bass/thump", ParamValueKind::Number, &kBandRange, nullptr},
            {1, "200 Hz", "Warmth/body", ParamValueKind::Number, &kBandRange, nullptr},
            {2, "400 Hz", "Low-mid", ParamValueKind::Number, &kBandRange, nullptr},
            {3, "800 Hz", "Midrange/punch", ParamValueKind::Number, &kBandRange, nullptr},
            {4, "1.6 kHz", "Upper-mid/bite", ParamValueKind::Number, &kBandRange, nullptr},
            {5, "3.2 kHz", "Presence/clarity", ParamValueKind::Number, &kBandRange, nullptr},
            {6, "6.4 kHz", "Treble/air", ParamValueKind::Number, &kBandRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Graphic EQ", "GEQ", "7-band EQ optimized for guitar.", kParams, 7};
    }

    //=========================================================================
    // Flanger
    //=========================================================================
    namespace Flanger
    {
        constexpr uint8_t TypeId = 19;
        inline const NumberParamRange kRateRange = {0.05f, 5.0f, 0.01f};
        inline const NumberParamRange kDepthRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kFeedbackRange = {-0.95f, 0.95f, 0.01f};
        inline const NumberParamRange kDelayRange = {0.1f, 10.0f, 0.1f};
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Rate", "LFO rate (Hz)", ParamValueKind::Number, &kRateRange, nullptr},
            {1, "Depth", "Modulation depth", ParamValueKind::Number, &kDepthRange, nullptr},
            {2, "Feedback", "Feedback (-95% to +95%)", ParamValueKind::Number, &kFeedbackRange, nullptr},
            {3, "Delay", "Base delay (ms)", ParamValueKind::Number, &kDelayRange, nullptr},
            {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Flanger", "FLG", "Classic jet/swoosh flanger.", kParams, 5};
    }

    //=========================================================================
    // Phaser
    //=========================================================================
    namespace Phaser
    {
        constexpr uint8_t TypeId = 20;
        inline const NumberParamRange kRateRange = {0.1f, 2.0f, 0.01f};
        inline const NumberParamRange kDepthRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kFeedbackRange = {0.0f, 0.75f, 0.01f};
        inline const NumberParamRange kFreqRange = {100.0f, 1600.0f, 1.0f};
        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const ParamInfo kParams[] = {
            {0, "Rate", "LFO rate (Hz)", ParamValueKind::Number, &kRateRange, nullptr},
            {1, "Depth", "Sweep depth", ParamValueKind::Number, &kDepthRange, nullptr},
            {2, "Feedback", "Resonance", ParamValueKind::Number, &kFeedbackRange, nullptr},
            {3, "Freq", "Base frequency (Hz)", ParamValueKind::Number, &kFreqRange, nullptr},
            {4, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Phaser", "PHS", "Classic sweeping phaser.", kParams, 5};
    }

    //=========================================================================
    // Neural Amp (NAM / RTNeural-based amp simulation)
    //=========================================================================
    namespace NeuralAmp
    {
        constexpr uint8_t TypeId = 21;

        // Model selection enum options (must match EmbeddedModels::Model order)
        inline const EnumParamOption kModelOptions[] = {
            {0, "Fender 57"},
            {1, "Matchless"},
            {2, "Klon BB"},
            {3, "Mesa IIC"},
            {4, "HAK Clean"},
            {5, "Bassman"},
            {6, "5150"},
            {7, "Splawn"},
        };
        inline const EnumParamInfo kModelEnum = {kModelOptions, 8};

        inline const NumberParamRange kGainRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kEqRange = {0.0f, 1.0f, 0.01f};

        inline const ParamInfo kParams[] = {
            {0, "Model", "Amp model selection", ParamValueKind::Enum, nullptr, &kModelEnum},
            {1, "Input", "Input gain/drive", ParamValueKind::Number, &kGainRange, nullptr},
            {2, "Output", "Output level", ParamValueKind::Number, &kGainRange, nullptr},
            {3, "Bass", "Low frequency boost/cut", ParamValueKind::Number, &kEqRange, nullptr},
            {4, "Mid", "Mid frequency boost/cut", ParamValueKind::Number, &kEqRange, nullptr},
            {5, "Treble", "High frequency boost/cut", ParamValueKind::Number, &kEqRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Neural Amp", "NAM", "AI-trained amp simulation (AIDA-X/RTNeural).", kParams, 6};
    }

    //=========================================================================
    // Cabinet IR (Impulse Response convolution for speaker cabinet simulation)
    //=========================================================================
    namespace CabinetIR
    {
        constexpr uint8_t TypeId = 22;

        // IR selection enum options (must match EmbeddedIRs::IR order)
        // NOTE: V1 is mono only. Stereo IR support deferred to V2.
        inline const EnumParamOption kIROptions[] = {
            {0, "V30 P1 Opus87"},
            {1, "V30 P1 Sene935"},
            {2, "V30 P2 Audix i5"},
            {3, "V30 P2 Sene935"},
            {4, "Mesa V30 SM57 1"},
            {5, "Mesa V30 SM57 2"},
            {6, "Mesa V30 SM58 1"},
            {7, "Mesa V30 SM58 2"},
            {8, "Mesa V30 AT2020 1"},
            {9, "Mesa V30 AT2020 2"},
            {10, "Mesa V30 SM57 Raw"},
            {11, "Mesa V30 SM58 Raw"},
            {12, "Mars Proteus"},
            {13, "Mars US Deluxe"},
            {14, "Mars Vox Bright"},
        };
        inline const EnumParamInfo kIREnum = {kIROptions, 15};

        inline const NumberParamRange kMixRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kOutputRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kLowCutRange = {0.0f, 1.0f, 0.01f};
        inline const NumberParamRange kHighCutRange = {0.0f, 1.0f, 0.01f};

        inline const ParamInfo kParams[] = {
            {0, "Cabinet", "Cabinet IR selection", ParamValueKind::Enum, nullptr, &kIREnum},
            {1, "Mix", "Wet/dry mix", ParamValueKind::Number, &kMixRange, nullptr},
            {2, "Output", "Output level (-20dB to +20dB)", ParamValueKind::Number, &kOutputRange, nullptr},
            {3, "Low Cut", "Cuts bass (0=off, 1=800Hz)", ParamValueKind::Number, &kLowCutRange, nullptr},
            {4, "High Cut", "Cuts treble (0=bright, 1=dark)", ParamValueKind::Number, &kHighCutRange, nullptr},
        };
        inline const ::EffectMeta kMeta = {"Cabinet IR", "CAB", "Impulse response convolution for speaker cabinet simulation.", kParams, 5};
    }

    //=========================================================================
    // Master list of all effects (ordered for UI display)
    //=========================================================================

    struct EffectEntry
    {
        uint8_t typeId;
        const ::EffectMeta *meta;
    };

    // This is the single source of truth for which effects exist and their order
    inline const EffectEntry kAllEffects[] = {
        {Off::TypeId, &Off::kMeta},
        {Delay::TypeId, &Delay::kMeta},
        {Distortion::TypeId, &Distortion::kMeta},
        {SweepDelay::TypeId, &SweepDelay::kMeta},
        {Mixer::TypeId, &Mixer::kMeta},
        {Reverb::TypeId, &Reverb::kMeta},
        {Compressor::TypeId, &Compressor::kMeta},
        {Chorus::TypeId, &Chorus::kMeta},
        {NoiseGate::TypeId, &NoiseGate::kMeta},
        {GraphicEQ::TypeId, &GraphicEQ::kMeta},
        {Flanger::TypeId, &Flanger::kMeta},
        {Phaser::TypeId, &Phaser::kMeta},
        {NeuralAmp::TypeId, &NeuralAmp::kMeta},
        {CabinetIR::TypeId, &CabinetIR::kMeta},
    };

    constexpr size_t kNumEffects = sizeof(kAllEffects) / sizeof(kAllEffects[0]);

    // Helper: find effect by type ID
    inline const ::EffectMeta *findByTypeId(uint8_t typeId)
    {
        for (size_t i = 0; i < kNumEffects; ++i)
        {
            if (kAllEffects[i].typeId == typeId)
                return kAllEffects[i].meta;
        }
        return nullptr;
    }

    // Helper: get index in kAllEffects by type ID (for combo box selection)
    inline int getIndexByTypeId(uint8_t typeId)
    {
        for (size_t i = 0; i < kNumEffects; ++i)
        {
            if (kAllEffects[i].typeId == typeId)
                return static_cast<int>(i);
        }
        return 0; // Default to Off
    }

    // Helper: get type ID from index
    inline uint8_t getTypeIdByIndex(size_t index)
    {
        if (index < kNumEffects)
            return kAllEffects[index].typeId;
        return Off::TypeId;
    }

} // namespace Effects
