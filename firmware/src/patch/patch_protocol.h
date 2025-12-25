
#pragma once
#include <cstdint>

static constexpr uint8_t ROUTE_INPUT = 255;

enum class ChannelPolicy : uint8_t { Auto=0, ForceMono=1, ForceStereo=2 };
enum class ButtonMode   : uint8_t { Unused=0, ToggleBypass=1, TapTempo=2 };

struct ButtonAssignWire { uint8_t slotIndex; ButtonMode mode; };
struct SlotParamWire { uint8_t id; uint8_t value; }; // 0..127

struct SlotWireDesc
{
    uint8_t slotIndex;
    uint8_t typeId;
    uint8_t enabled;

    uint8_t inputL;
    uint8_t inputR;

    uint8_t sumToMono; // 0/1
    uint8_t dry;       // 0..127
    uint8_t wet;       // 0..127

    uint8_t channelPolicy; // ChannelPolicy
    uint8_t numParams;     // <= 8
    SlotParamWire params[8];
};

struct PatchWireDesc
{
    uint8_t numSlots;
    SlotWireDesc slots[12];
    ButtonAssignWire buttons[2];
};
