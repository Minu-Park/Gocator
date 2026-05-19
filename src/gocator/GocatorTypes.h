#pragma once

#include <cstdint>
#include <string>

namespace gocator
{

constexpr std::uint16_t kDefaultControlPort = 3600;
constexpr std::uint16_t kDefaultGdpPort = 3601;
constexpr std::uint16_t kDefaultWebPort = 8100;
constexpr int kDefaultCommandTimeoutMs = 30000;

struct GocatorConnectionConfig
{
    std::string address;
    std::uint16_t controlPort = kDefaultControlPort;
    int commandTimeoutMs = kDefaultCommandTimeoutMs;
};

} // namespace gocator
