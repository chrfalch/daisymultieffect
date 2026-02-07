#include "NetworkMidiSetup.h"

#include <CoreMIDI/MIDINetworkSession.h>
#include <cstdlib>
#include <sstream>

NetworkMidiSetupResult NetworkMidiSetup::configureFromEnvironment()
{
    NetworkMidiSetupResult result;

    MIDINetworkSession *session = [MIDINetworkSession defaultSession];
    if (session == nil)
    {
        result.summary = "Network MIDI session is unavailable on this system.";
        return result;
    }

    result.sessionAvailable = true;
    session.enabled = YES;
    session.connectionPolicy = MIDINetworkConnectionPolicy_Anyone;
    result.sessionEnabled = session.isEnabled;

    const char *hostValue = std::getenv("DAISY_MIDI_IPAD_HOST");
    if (hostValue == nullptr || hostValue[0] == '\0')
    {
        result.summary = "Enabled macOS Network MIDI session. Set DAISY_MIDI_IPAD_HOST to auto-connect to iPad.";
        return result;
    }

    result.hostConnectAttempted = true;

    int port = 5004;
    if (const char *portValue = std::getenv("DAISY_MIDI_IPAD_PORT"))
    {
        const int parsedPort = std::atoi(portValue);
        if (parsedPort > 0 && parsedPort <= 65535)
            port = parsedPort;
    }

    const char *nameValue = std::getenv("DAISY_MIDI_IPAD_NAME");
    NSString *hostName = [NSString stringWithUTF8String:(nameValue != nullptr && nameValue[0] != '\0') ? nameValue : "iPad"];
    NSString *hostAddress = [NSString stringWithUTF8String:hostValue];

    MIDINetworkHost *host = [MIDINetworkHost hostWithName:hostName
                                                   address:hostAddress
                                                      port:static_cast<UInt16>(port)];
    if (host == nil)
    {
        result.summary = "Network MIDI enabled, but failed to create iPad host entry.";
        return result;
    }

    [session addContact:host];
    MIDINetworkConnection *connection = [MIDINetworkConnection connectionWithHost:host];
    [session addConnection:connection];

    BOOL connected = NO;
    for (MIDINetworkConnection *activeConnection in session.connections)
    {
        if ([activeConnection.host.address isEqualToString:host.address] && activeConnection.host.port == host.port)
        {
            connected = YES;
            break;
        }
    }

    result.hostConnected = connected;

    std::ostringstream msg;
    msg << "Enabled Network MIDI session and attempted iPad connect to "
        << hostValue << ":" << port;
    if (connected)
        msg << " (connected).";
    else
        msg << " (pending/failed - verify iPad app is advertising RTP-MIDI).";
    result.summary = msg.str();

    return result;
}
