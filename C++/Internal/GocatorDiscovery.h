#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GocatorTypes.h"

namespace gocator
{

struct GocatorDiscoveryOptions
{
    std::uint64_t timeoutMs = 3000;
    bool classicDiscover = false;
};

struct GocatorDeviceInfo
{
    std::string address;
    std::string appId;
    std::string appName;
    std::string appVersion;
    std::string deviceModel;
    std::string gateway;
    std::string mask;
    std::uint16_t controlPort = kDefaultControlPort;
    std::uint16_t gdpPort = kDefaultGdpPort;
    std::uint16_t webPort = kDefaultWebPort;
    std::uint32_t serialNumber = 0;
    bool dhcp = false;
    bool addressConflict = false;
    bool remote = false;
    int hmiStatus = 0;

    GocatorConnectionConfig connectionConfig(int commandTimeoutMs = kDefaultCommandTimeoutMs) const;
    bool canConnectLocally() const;
};

class GocatorDiscovery
{
public:
    std::vector<GocatorDeviceInfo> discover(const GocatorDiscoveryOptions& options = {}) const;

    static GocatorConnectionConfig manualTarget(
        std::string address,
        std::uint16_t controlPort = kDefaultControlPort,
        int commandTimeoutMs = kDefaultCommandTimeoutMs);
};

} // namespace gocator
