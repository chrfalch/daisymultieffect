
#pragma once

// Include the shared protocol definitions
#include "protocol/sysex_protocol.h"

// Re-export commonly used types and constants at global scope for VST compatibility
using DaisyMultiFX::Protocol::ROUTE_INPUT;
using DaisyMultiFX::Protocol::MAX_SLOTS;
using DaisyMultiFX::Protocol::MAX_PARAMS_PER_SLOT;
using DaisyMultiFX::Protocol::NUM_BUTTONS;
using DaisyMultiFX::Protocol::MANUFACTURER_ID;
using DaisyMultiFX::Protocol::ChannelPolicy;
using DaisyMultiFX::Protocol::ButtonMode;
using DaisyMultiFX::Protocol::ButtonAssignWire;
using DaisyMultiFX::Protocol::SlotParamWire;
using DaisyMultiFX::Protocol::SlotWireDesc;
using DaisyMultiFX::Protocol::PatchWireDesc;

// Utility functions
using DaisyMultiFX::Protocol::floatToQ16_16;
using DaisyMultiFX::Protocol::q16_16ToFloat;
using DaisyMultiFX::Protocol::packQ16_16;
using DaisyMultiFX::Protocol::unpackQ16_16;
using DaisyMultiFX::Protocol::encodeRoute;
using DaisyMultiFX::Protocol::decodeRoute;

// Bring command/response constants into scope
namespace SysExCmd = DaisyMultiFX::Protocol::Command;
namespace SysExResp = DaisyMultiFX::Protocol::Response;
namespace EffectType = DaisyMultiFX::Protocol::EffectType;
