#pragma once

#include <stdint.h>

#include "tempo.h"

class MidiControl;

class TempoControl
{
public:
    void Init(TempoSource &tempo, MidiControl *midi);

    // Pass a monotonic timestamp in microseconds.
    void Tap(uint32_t nowUs);

    void Reset();

private:
    TempoSource *tempo_ = nullptr;
    MidiControl *midi_ = nullptr;
    uint32_t lastTapUs_ = 0;
    uint32_t avgTapUs_ = 0;
};
