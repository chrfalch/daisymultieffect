#pragma once

#include <string>

struct NetworkMidiSetupResult
{
    bool sessionAvailable = false;
    bool sessionEnabled = false;
    bool hostConnectAttempted = false;
    bool hostConnected = false;
    std::string summary;
};

class NetworkMidiSetup
{
public:
    static NetworkMidiSetupResult configureFromEnvironment();
};
