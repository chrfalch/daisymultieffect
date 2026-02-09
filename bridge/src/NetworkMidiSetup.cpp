#include "NetworkMidiSetup.h"

#if !defined(__APPLE__)
NetworkMidiSetupResult NetworkMidiSetup::configureFromEnvironment()
{
    NetworkMidiSetupResult result;
    result.summary = "Automatic Network MIDI setup is only available on macOS.";
    return result;
}
#endif
