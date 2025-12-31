//
//  DaisyMultiFX-Bridging-Header.h
//  DaisyMultiFX macOS Control App
//
//  This bridging header imports the C protocol definitions so Swift
//  can use the same constants as the firmware and VST plugin.
//

#ifndef DaisyMultiFX_Bridging_Header_h
#define DaisyMultiFX_Bridging_Header_h

// Note: Swift currently uses native Swift constants in SysExProtocol.swift
// instead of importing from C header due to Xcode bridging limitations.
// If bridging is set up correctly in Xcode, you can uncomment this:
// #include "core/protocol/sysex_protocol_c.h"

#endif /* DaisyMultiFX_Bridging_Header_h */
