# DaisyMultiFX Documentation

This directory contains technical documentation for the DaisyMultiFX project.

## Analysis & Design Documents

### MIDI Control Change Analysis (NEW)
- **[midi-cc-summary.md](./midi-cc-summary.md)** - Executive summary for stakeholders (8 KB)
  - Quick overview of proposed changes
  - Key benefits and recommendations
  - Implementation phases and timeline
  - **Start here** for decision-making

- **[midi-cc-analysis.md](./midi-cc-analysis.md)** - Comprehensive technical analysis (21 KB)
  - Complete protocol analysis
  - Detailed design specifications
  - Risk assessment and mitigation
  - Testing strategy
  - **Read this** for implementation details

- **[midi-cc-architecture.md](./midi-cc-architecture.md)** - System architecture diagrams (34 KB)
  - Visual flow diagrams
  - Memory layout diagrams
  - Signal flow illustrations
  - **Use this** for understanding system design

### Existing Documentation
- **[adding-effects.md](./adding-effects.md)** - Guide for adding new effects
- **[ux-routing-wireframe.md](./ux-routing-wireframe.md)** - UI routing design
- **[effects/](./effects/)** - Individual effect documentation

## Quick Links

### For Product Owners / Stakeholders
Read these in order:
1. [midi-cc-summary.md](./midi-cc-summary.md) - Understand what's being proposed
2. Make decision: Approve or request changes
3. If approved: Implementation team proceeds with Phase 1

### For Developers / Implementers
Read these in order:
1. [midi-cc-summary.md](./midi-cc-summary.md) - Get high-level overview
2. [midi-cc-analysis.md](./midi-cc-analysis.md) - Study detailed design
3. [midi-cc-architecture.md](./midi-cc-architecture.md) - Understand system architecture
4. Refer to [../architecture.md](../architecture.md) for overall system design

### For Reviewers / Testers
Key sections to focus on:
- Testing Strategy (Section 7 in analysis.md)
- Hardware Requirements (Summary in midi-cc-summary.md)
- Potential Issues (Section 6 in analysis.md)

## Document Status

| Document | Version | Status | Last Updated |
|----------|---------|--------|--------------|
| midi-cc-summary.md | 1.0 | ✅ Ready for Review | 2024-02-06 |
| midi-cc-analysis.md | 1.0 | ✅ Ready for Review | 2024-02-06 |
| midi-cc-architecture.md | 1.0 | ✅ Ready for Review | 2024-02-06 |

## Summary of Proposed Changes

### What's Being Added
1. **Standard MIDI CC Support**
   - Use any MIDI controller (Korg, Behringer, etc.)
   - Real-time parameter control
   - Bank Select + Program Change

2. **Patch Storage**
   - 128-256 patches stored in QSPI flash
   - Persistent across power cycles
   - Organized in banks (8 banks × 16 patches)

### What Stays the Same
- ✅ Existing SysEx protocol (no breaking changes)
- ✅ Controller app continues to work
- ✅ All current features preserved
- ✅ Bidirectional sync maintained

### Key Design Decisions
- **Hybrid Approach:** Both SysEx and CC protocols coexist
- **SysEx for Programming:** Full control, metadata, bidirectional sync
- **CC for Performance:** Real-time control, standard MIDI controllers
- **Memory Allocation:** 256 KB of QSPI flash (3% of available)
- **Implementation Time:** 8-11 days (3 phases)

## Next Steps

1. ✅ Analysis complete
2. ⏳ Stakeholder review (waiting)
3. ⏳ Approval decision
4. ⏳ Phase 1 implementation (2-3 days)
5. ⏳ Phase 2 implementation (4-5 days)
6. ⏳ Phase 3 implementation (2-3 days)

## Questions or Feedback?

For questions about this analysis:
- Review the [FAQ section in midi-cc-analysis.md](./midi-cc-analysis.md#10-conclusion)
- Check the [Potential Issues section](./midi-cc-analysis.md#6-potential-issues-and-solutions)
- Open an issue on GitHub with label `midi-cc-analysis`

## Related Files

### Core Protocol Files
- `core/protocol/midi_protocol.h` - Current SysEx protocol (598 lines)
- `core/protocol/sysex_protocol.h` - Protocol definitions (190 lines)
- `firmware/src/midi/midi_control.cpp` - MIDI control implementation (1043 lines)
- `firmware/src/midi/midi_control.h` - MIDI control interface (133 lines)

### Implementation Will Add
- `core/protocol/cc_mapping.h` - NEW: CC mapping table
- `firmware/src/storage/flash_storage.cpp` - NEW: Flash storage driver
- `core/storage/storage_layout.h` - NEW: Storage format definitions

---

**Document Index Version:** 1.0  
**Last Updated:** 2024-02-06  
**Maintained By:** DaisyMultiFX Team
