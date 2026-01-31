
#pragma once
#include <cstdint>

struct ParamDesc
{
    uint8_t id;
    uint8_t value;
}; // 0..127 snapshot

struct OutputParamDesc
{
    uint8_t id;
    float value; // Raw float value (Q16.16 encoded for SysEx)
};

enum class ChannelMode : uint8_t
{
    Mono,
    Stereo,
    MonoOrStereo
};

enum class ParamValueKind : uint8_t
{
    Number = 0,
    Enum = 1,
    File = 2 // File path selector (e.g., JSON model files)
};

struct NumberParamRange
{
    float minValue;
    float maxValue;
    float step;
};
struct EnumParamOption
{
    uint8_t value;
    const char *name;
};
struct EnumParamInfo
{
    const EnumParamOption *options;
    uint8_t numOptions;
};

struct ParamInfo
{
    uint8_t id;
    const char *name;
    const char *description;
    ParamValueKind kind;
    const NumberParamRange *number;   // if Number
    const EnumParamInfo *enumeration; // if Enum
    const char *unit = nullptr;       // Display unit suffix (e.g. "s", "ms", "dB", "Hz")
    bool isDisplayParam = false;      // If true, app shows this param's current value label on the pedal
    bool isReadonly = false;          // If true, firmware writes value, app displays only
};

struct EffectMeta
{
    const char *name;
    const char *shortName; // 3-character short name for display/MIDI
    const char *description;
    const ParamInfo *params;
    uint8_t numParams;
    bool isGlobal = false; // If true, routes audio exclusively to this slot when enabled
};

struct BaseEffect
{
    virtual ~BaseEffect() {}
    virtual uint8_t GetTypeId() const = 0;
    virtual ChannelMode GetSupportedModes() const = 0;

    virtual void Init(float sampleRate) = 0;
    virtual void ProcessStereo(float &l, float &r) = 0;

    virtual void ProcessMono(float &m)
    {
        float l = m, r = m;
        ProcessStereo(l, r);
        m = 0.5f * (l + r);
    }

    virtual void SetParam(uint8_t id, float v01) = 0; // v in [0,1]
    virtual uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const = 0;

    virtual const EffectMeta &GetMetadata() const = 0;

    // Return current values for readonly/output params.
    // Called from main loop (not ISR), safe to read effect state.
    virtual uint8_t GetOutputParams(OutputParamDesc *out, uint8_t max) const { return 0; }
};
