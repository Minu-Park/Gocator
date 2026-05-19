#include <exception>
#include <iostream>
#include <string>

#include "gocator/GocatorDiscovery.h"
#include "gocator/GocatorSettingsManager.h"

namespace
{

void printUsage(const char* app)
{
    std::cerr << "Usage:\n"
              << "  " << app << " discover [timeout-ms]\n"
              << "  " << app << " <sensor-ip> info\n"
              << "  " << app << " <sensor-ip> read <resource-path>\n"
              << "  " << app << " <sensor-ip> profile-output\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "discover")
    {
        try
        {
            gocator::GocatorDiscoveryOptions options;
            if (argc >= 3)
            {
                options.timeoutMs = std::stoull(argv[2]);
            }

            const gocator::GocatorDiscovery discovery;
            const std::vector<gocator::GocatorDeviceInfo> devices = discovery.discover(options);

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

            return 0;
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
        gocator::GocatorSettingsManager settings({address});
        settings.connect();

        if (command == "info")
        {
            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            std::cout << "model=" << scanner.model << '\n'
                      << "serial=" << scanner.serialNumber << '\n'
                      << "engine=" << scanner.engineId << '\n'
                      << "scannerPath=" << scanner.scannerPath << '\n'
                      << "profileSource=" << scanner.profileSourceId << '\n';
            return 0;
        }

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
