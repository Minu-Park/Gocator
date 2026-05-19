#include <exception>
#include <iostream>
#include <string>

#include "gocator/GocatorDiscovery.h"
#include "gocator/GocatorSettingsManager.h"

namespace
{

constexpr std::uint64_t kDefaultDiscoveryTimeoutMs = 3000;
constexpr const char* kDefaultManualAddress = "192.168.1.10";

void printUsage(const char* app)
{
    std::cerr << "Usage:\n"
              << "  " << app << "                # discover, then fallback to " << kDefaultManualAddress << " info\n"
              << "  " << app << " discover [timeout-ms]\n"
              << "  " << app << " <sensor-ip> info\n"
              << "  " << app << " <sensor-ip> read <resource-path>\n"
              << "  " << app << " <sensor-ip> profile-output\n";
}

int runDiscover(std::uint64_t timeoutMs)
{
    gocator::GocatorDiscoveryOptions options;
    options.timeoutMs = timeoutMs;

    const gocator::GocatorDiscovery discovery;
    const std::vector<gocator::GocatorDeviceInfo> devices = discovery.discover(options);

    std::cout << "found=" << devices.size() << '\n';
    for (std::size_t i = 0; i < devices.size(); ++i)
    {
        const gocator::GocatorDeviceInfo& device = devices[i];
        std::cout << "index=" << i
                  << " ip=" << device.address
                  << " controlPort=" << device.controlPort
                  << " gdpPort=" << device.gdpPort
                  << " webPort=" << device.webPort
                  << " serial=" << device.serialNumber
                  << " model=" << device.deviceModel
                  << " dhcp=" << device.dhcp
                  << " remote=" << device.remote
                  << " conflict=" << device.addressConflict
                  << " localConnect=" << device.canConnectLocally()
                  << '\n';
    }

    return devices.empty() ? 1 : 0;
}

int runInfo(const std::string& address)
{
    gocator::GocatorSettingsManager settings({address});
    settings.connect();

    const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
    std::cout << "model=" << scanner.model << '\n'
              << "serial=" << scanner.serialNumber << '\n'
              << "engine=" << scanner.engineId << '\n'
              << "scannerPath=" << scanner.scannerPath << '\n'
              << "profileSource=" << scanner.profileSourceId << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        try
        {
            const int discoveryResult = runDiscover(kDefaultDiscoveryTimeoutMs);
            if (discoveryResult == 0)
            {
                return 0;
            }

            std::cerr << "Discovery found no devices; trying manual target "
                      << kDefaultManualAddress << '\n';
            return runInfo(kDefaultManualAddress);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Discovery error: " << e.what() << '\n'
                      << "Trying manual target " << kDefaultManualAddress << '\n';
            try
            {
                return runInfo(kDefaultManualAddress);
            }
            catch (const std::exception& manualError)
            {
                std::cerr << "Manual target error: " << manualError.what() << '\n';
                return 1;
            }
        }
    }

    if (argc >= 2 && std::string(argv[1]) == "discover")
    {
        try
        {
            const std::uint64_t timeoutMs = (argc >= 3)
                ? std::stoull(argv[2])
                : kDefaultDiscoveryTimeoutMs;
            return runDiscover(timeoutMs);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << '\n';
            return 1;
        }
    }

    if (argc < 3)
    {
        printUsage(argv[0]);
        return 2;
    }

    const std::string address = argv[1];
    const std::string command = argv[2];

    try
    {
        if (command == "info")
        {
            return runInfo(address);
        }

        gocator::GocatorSettingsManager settings({address});
        settings.connect();

        if (command == "read")
        {
            if (argc < 4)
            {
                printUsage(argv[0]);
                return 2;
            }

            std::cout << settings.read(argv[3]).ToString() << '\n';
            return 0;
        }

        if (command == "profile-output")
        {
            const gocator::ScannerInfo scanner = settings.prepareProfileOutput();
            std::cout << "configuredProfileSource=" << scanner.profileSourceId << '\n';
            return 0;
        }

        printUsage(argv[0]);
        return 2;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
