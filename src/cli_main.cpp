#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "gocator/GocatorAcquisition.h"
#include "gocator/GocatorDiscovery.h"
#include "gocator/GocatorSettingsManager.h"
#include "gocator/GocatorParameterSet.h"

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
              << "  " << app << " <sensor-ip> update <resource-path> <json-payload>\n"
              << "  " << app << " <sensor-ip> scanner-read\n"
              << "  " << app << " <sensor-ip> scanner-schema\n"
              << "  " << app << " <sensor-ip> params-all\n"
              << "  " << app << " <sensor-ip> params-flat\n"
              << "  " << app << " <sensor-ip> tool-list\n"
              << "  " << app << " <sensor-ip> tool-params <tool-id>\n"
              << "  " << app << " <sensor-ip> config-save <filename.json>\n"
              << "  " << app << " <sensor-ip> config-load <filename.json>\n"
              << "  " << app << " <sensor-ip> tune <scan-mode> <intensity 0|1> <uniform 0|1> [exposure|-]\n"
              << "  " << app << " <sensor-ip> profile-output\n"
              << "  " << app << " <sensor-ip> grab [receive-ms] [frames]\n"
              << "  " << app << " <sensor-ip> profile-test [scan-mode] [intensity 0|1] [uniform 0|1] [exposure|-] [receive-ms] [frames]\n"
              << "  " << app << " <sensor-ip> source-test <scan-mode> <source-id> [receive-ms] [frames]\n"
              << "  " << app << " <sensor-ip> sweep-exposure <start> <stop> <step> [scan-mode] [intensity 0|1] [uniform 0|1] [receive-ms] [frames]\n"
              << "  " << app << " <sensor-ip> sweep-z <start-mm> <stop-mm> <step-mm> [exposure|-] [receive-ms] [frames]\n"
              << "  " << app << " <sensor-ip> sweep-threshold <start> <stop> <step> [exposure|-] [receive-ms] [frames]\n";
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
              << "sensorPath=" << scanner.sensorPath << '\n'
              << "profileSource=" << scanner.profileSourceId << '\n';
    return 0;
}

bool parseBoolArg(const std::string& value)
{
    if (value == "1" || value == "true" || value == "on")
    {
        return true;
    }

    if (value == "0" || value == "false" || value == "off")
    {
        return false;
    }

    throw std::invalid_argument("Invalid bool value: " + value);
}

void printFrame(const gocator::GocatorFrame& frame)
{
    std::cout << "messages=" << frame.messageCount
              << " images=" << frame.images.size()
              << " profiles=" << frame.profiles.size()
              << " spots=" << frame.spots.size() << '\n';

    for (const gocator::GocatorFrameMessage& message : frame.messages)
    {
        std::cout << "message type=" << message.typeName
                  << " value=" << message.typeValue
                  << " source=" << message.sourceId
                  << " dataSet=" << message.dataSetId
                  << " gdpId=" << message.gdpId
                  << " last=" << message.isLastMessage << '\n';
    }

    for (const gocator::GocatorImageFrame& image : frame.images)
    {
        std::cout << "image width=" << image.width
                  << " height=" << image.height
                  << " pixelSize=" << image.pixelSize
                  << " format=" << image.pixelFormatName
                  << " bytes=" << image.dataSize
                  << " source=" << image.sourceId;
        if (image.hasByteStats)
        {
            std::cout << " byte[min,max,mean]="
                      << static_cast<int>(image.minByte) << ","
                      << static_cast<int>(image.maxByte) << ","
                      << image.meanByte;
        }
        std::cout << '\n';
    }

    for (const gocator::GocatorProfileFrame& profile : frame.profiles)
    {
        std::cout << "profile points=" << profile.width
                  << " valid=" << profile.validCount
                  << " null=" << profile.nullCount
                  << " xRes=" << profile.xResolution
                  << " zRes=" << profile.zResolution
                  << " source=" << profile.sourceId;

        if (profile.hasRangeStats)
        {
            std::cout << " range[first,min,max]="
                      << profile.firstRange << ","
                      << profile.minRange << ","
                      << profile.maxRange;
        }

        if (profile.hasIntensityStats)
        {
            std::cout << " intensity[min,max]="
                      << profile.minIntensity << ","
                      << profile.maxIntensity;
        }

        std::cout << '\n';
    }

    for (const gocator::GocatorSpotsFrame& spots : frame.spots)
    {
        std::cout << "spots points=" << spots.pointCount
                  << " exposure=" << spots.exposure
                  << " columnBased=" << spots.columnBased
                  << " maxSliceCount=" << spots.maxSliceCount
                  << " center[min,max]=" << spots.spotCenterMin << ","
                  << spots.spotCenterMax
                  << " source=" << spots.sourceId << '\n';
    }
}

gocator::ScanTuningOptions parseTuning(int argc, char** argv, int startIndex)
{
    gocator::ScanTuningOptions options;

    if (argc > startIndex)
    {
        options.profileMode.scanMode = std::stoi(argv[startIndex]);
    }
    if (argc > startIndex + 1)
    {
        options.profileMode.intensityEnabled = parseBoolArg(argv[startIndex + 1]);
    }
    if (argc > startIndex + 2)
    {
        options.profileMode.uniformSpacingEnabled = parseBoolArg(argv[startIndex + 2]);
    }
    if (argc > startIndex + 3 && std::string(argv[startIndex + 3]) != "-")
    {
        options.updateExposure = true;
        options.exposure = std::stoi(argv[startIndex + 3]);
    }

    return options;
}

int runGrab(const std::string& address, int receiveMs, int frames)
{
    gocator::GocatorAcquisition acquisition({address});
    const gocator::GocatorFrame frame = acquisition.grabUntilValidProfile(receiveMs, frames);
    printFrame(frame);
    return frame.profiles.empty() && frame.images.empty() ? 1 : 0;
}

int runProfileTest(const std::string& address, const gocator::ScanTuningOptions& options, int receiveMs, int frames)
{
    gocator::GocatorSettingsManager settings({address});
    settings.connect();
    gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
    settings.stopIfRunning();
    settings.configureScanTuning(scanner, options);
    settings.enableGocatorProtocol(true);
    settings.clearGocatorOutputs();
    settings.addOutput(scanner.profileSourceId);

    std::cout << "configured source=" << scanner.profileSourceId
              << " scanMode=" << options.profileMode.scanMode
              << " intensity=" << options.profileMode.intensityEnabled
              << " uniform=" << options.profileMode.uniformSpacingEnabled;
    if (options.updateExposure)
    {
        std::cout << " exposure=" << options.exposure;
    }
    std::cout << '\n';

    return runGrab(address, receiveMs, frames);
}

int runSourceTest(
    const std::string& address,
    int scanMode,
    const std::string& sourceId,
    int receiveMs,
    int frames)
{
    gocator::GocatorSettingsManager settings({address});
    settings.connect();
    gocator::ScannerInfo scanner = settings.detectPrimaryScanner();

    gocator::ScanTuningOptions options;
    options.profileMode.scanMode = scanMode;
    settings.stopIfRunning();
    settings.configureScanTuning(scanner, options);
    settings.enableGocatorProtocol(true);
    settings.clearGocatorOutputs();
    settings.addOutput(sourceId);

    std::cout << "configured source=" << sourceId
              << " scanMode=" << scanMode << '\n';

    return runGrab(address, receiveMs, frames);
}

int runSweepExposure(
    const std::string& address,
    int start,
    int stop,
    int step,
    const gocator::ScanTuningOptions& baseOptions,
    int receiveMs,
    int frames)
{
    if (step == 0)
    {
        throw std::invalid_argument("Exposure step must not be zero");
    }

    const bool increasing = stop >= start;
    if ((increasing && step < 0) || (!increasing && step > 0))
    {
        step = -step;
    }

    for (int exposure = start; increasing ? exposure <= stop : exposure >= stop; exposure += step)
    {
        gocator::ScanTuningOptions options = baseOptions;
        options.updateExposure = true;
        options.exposure = exposure;

        std::cout << "=== exposure=" << exposure << " ===\n";
        try
        {
            runProfileTest(address, options, receiveMs, frames);
        }
        catch (const std::exception& e)
        {
            std::cout << "error=" << e.what() << '\n';
        }
    }

    return 0;
}

int runSweepZ(
    const std::string& address,
    double start,
    double stop,
    double step,
    const gocator::ScanTuningOptions& baseOptions,
    int receiveMs,
    int frames)
{
    if (step == 0.0)
    {
        throw std::invalid_argument("Z step must not be zero");
    }

    const bool increasing = stop >= start;
    if ((increasing && step < 0.0) || (!increasing && step > 0.0))
    {
        step = -step;
    }

    for (double z = start; increasing ? z <= stop + 1e-9 : z >= stop - 1e-9; z += step)
    {
        gocator::ScanTuningOptions options = baseOptions;
        options.updateActiveArea = true;
        options.activeAreaZ = z;
        options.activeAreaHeight = 1.1;

        std::cout << "=== z=" << z << " ===\n";
        try
        {
            runProfileTest(address, options, receiveMs, frames);
        }
        catch (const std::exception& e)
        {
            std::cout << "error=" << e.what() << '\n';
        }
    }

    return 0;
}

int runSweepThreshold(
    const std::string& address,
    int start,
    int stop,
    int step,
    const gocator::ScanTuningOptions& baseOptions,
    int receiveMs,
    int frames)
{
    if (step == 0)
    {
        throw std::invalid_argument("Threshold step must not be zero");
    }

    const bool increasing = stop >= start;
    if ((increasing && step < 0) || (!increasing && step > 0))
    {
        step = -step;
    }

    for (int threshold = start; increasing ? threshold <= stop : threshold >= stop; threshold += step)
    {
        gocator::ScanTuningOptions options = baseOptions;
        options.updateFlexSpotThreshold = true;
        options.flexSpotThreshold = threshold;

        std::cout << "=== threshold=" << threshold << " ===\n";
        try
        {
            runProfileTest(address, options, receiveMs, frames);
        }
        catch (const std::exception& e)
        {
            std::cout << "error=" << e.what() << '\n';
        }
    }

    return 0;
}

} // namespace

int runDebug(const std::string& address)
{
    std::cout << "Starting diagnostic for " << address << "...\n";
    try
    {
        gocator::GocatorSettingsManager settings({address});
        std::cout << "[1/5] Connecting to Control Channel... ";
        settings.connect();
        std::cout << "OK\n";

        std::cout << "[2/5] Detecting Scanner... ";
        gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
        std::cout << "OK (" << scanner.model << ", S/N: " << scanner.serialNumber << ")\n";

        std::cout << "[3/5] Checking System State... ";
        GoPxLSdk::GoJson systemObj = settings.read("/system");
        int runState = systemObj.At("/runState").Get<int>();
        std::string stateStr = (runState == 0 ? "Ready" : (runState == 1 ? "Running" : "Conflict"));
        std::cout << stateStr << "\n";

        std::cout << "[4/5] Checking Gocator Protocol... ";
        GoPxLSdk::GoJson control = settings.read("/controls/gocator");
        bool enabled = control.At("/enabled").Get<bool>();
        std::cout << (enabled ? "ENABLED" : "DISABLED") << "\n";

        std::cout << "[5/5] Checking Outputs... ";
        GoPxLSdk::GoJson outputsRoot = settings.read("/controls/gocator/outputs");
        GoPxLSdk::GoJson outputsMap = outputsRoot.At("/map");
        std::cout << "Found " << outputsMap.Size() << " configured outputs.\n";
        for (std::size_t i = 0; i < outputsMap.Size(); ++i)
        {
            std::cout << "  - [" << i << "] " << outputsMap.At("/" + std::to_string(i) + "/source").Get<std::string>() << "\n";
        }

        if (runState != 1)
        {
            std::cout << "\nWARNING: Sensor is NOT RUNNING (runState=" << runState << "). GDP data will not be sent.\n";
            std::cout << "Try running: ./gocator " << address << " grab\n";
        }

        std::cout << "\n[6/6] Attempting GDP Data Acquisition (5s timeout)... ";
        gocator::GocatorAcquisition acquisition({address});
        acquisition.connect();
        try 
        {
            gocator::GocatorFrame frame = acquisition.receiveOne(5000);
            std::cout << "SUCCESS\n";
            printFrame(frame);
        }
        catch (const std::exception& e)
        {
            std::cout << "FAILED: " << e.what() << "\n";
            if (runState != 1)
            {
                std::cout << "Reason: Sensor is not running. Use 'grab' or 'profile-test' to start acquisition.\n";
            }
            else if (!enabled)
            {
                std::cout << "Reason: Gocator Protocol is disabled.\n";
            }
            else if (outputsMap.Size() == 0)
            {
                std::cout << "Reason: No outputs are configured in the Gocator Protocol.\n";
            }
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nDIAGNOSTIC CRITICAL FAILURE: " << e.what() << "\n";
        return 1;
    }
}

int run_cli(int argc, char** argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        printUsage(argv[0]);
        return 0;
    }
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
            return runDebug(kDefaultManualAddress);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Discovery error: " << e.what() << '\n'
                      << "Trying manual target " << kDefaultManualAddress << '\n';
            try
            {
                return runDebug(kDefaultManualAddress);
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
        if (command == "debug")
        {
            return runDebug(address);
        }

        if (command == "info")
        {
            return runInfo(address);
        }

        if (command == "grab")
        {
            const int receiveMs = (argc >= 4) ? std::stoi(argv[3]) : 10000;
            const int frames = (argc >= 5) ? std::stoi(argv[4]) : 10;
            return runGrab(address, receiveMs, frames);
        }

        if (command == "profile-test")
        {
            const gocator::ScanTuningOptions options = parseTuning(argc, argv, 3);
            const int receiveMs = (argc >= 8) ? std::stoi(argv[7]) : 10000;
            const int frames = (argc >= 9) ? std::stoi(argv[8]) : 10;
            return runProfileTest(address, options, receiveMs, frames);
        }

        if (command == "source-test")
        {
            if (argc < 5)
            {
                printUsage(argv[0]);
                return 2;
            }

            const int scanMode = std::stoi(argv[3]);
            const std::string sourceId = argv[4];
            const int receiveMs = (argc >= 6) ? std::stoi(argv[5]) : 10000;
            const int frames = (argc >= 7) ? std::stoi(argv[6]) : 10;
            return runSourceTest(address, scanMode, sourceId, receiveMs, frames);
        }

        if (command == "sweep-exposure")
        {
            if (argc < 6)
            {
                printUsage(argv[0]);
                return 2;
            }

            const int start = std::stoi(argv[3]);
            const int stop = std::stoi(argv[4]);
            const int step = std::stoi(argv[5]);

            gocator::ScanTuningOptions options;
            if (argc >= 7)
            {
                options.profileMode.scanMode = std::stoi(argv[6]);
            }
            if (argc >= 8)
            {
                options.profileMode.intensityEnabled = parseBoolArg(argv[7]);
            }
            if (argc >= 9)
            {
                options.profileMode.uniformSpacingEnabled = parseBoolArg(argv[8]);
            }

            const int receiveMs = (argc >= 10) ? std::stoi(argv[9]) : 10000;
            const int frames = (argc >= 11) ? std::stoi(argv[10]) : 10;
            return runSweepExposure(address, start, stop, step, options, receiveMs, frames);
        }

        if (command == "sweep-z")
        {
            if (argc < 6)
            {
                printUsage(argv[0]);
                return 2;
            }

            const double start = std::stod(argv[3]);
            const double stop = std::stod(argv[4]);
            const double step = std::stod(argv[5]);

            gocator::ScanTuningOptions options;
            if (argc >= 7 && std::string(argv[6]) != "-")
            {
                options.updateExposure = true;
                options.exposure = std::stoi(argv[6]);
            }

            const int receiveMs = (argc >= 8) ? std::stoi(argv[7]) : 10000;
            const int frames = (argc >= 9) ? std::stoi(argv[8]) : 10;
            return runSweepZ(address, start, stop, step, options, receiveMs, frames);
        }

        if (command == "sweep-threshold")
        {
            if (argc < 6)
            {
                printUsage(argv[0]);
                return 2;
            }

            const int start = std::stoi(argv[3]);
            const int stop = std::stoi(argv[4]);
            const int step = std::stoi(argv[5]);

            gocator::ScanTuningOptions options;
            if (argc >= 7 && std::string(argv[6]) != "-")
            {
                options.updateExposure = true;
                options.exposure = std::stoi(argv[6]);
            }

            const int receiveMs = (argc >= 8) ? std::stoi(argv[7]) : 10000;
            const int frames = (argc >= 9) ? std::stoi(argv[8]) : 10;
            return runSweepThreshold(address, start, stop, step, options, receiveMs, frames);
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

        if (command == "update")
        {
            if (argc < 5)
            {
                printUsage(argv[0]);
                return 2;
            }

            settings.update(argv[3], GoPxLSdk::GoJson(argv[4]));
            std::cout << "updated=" << argv[3] << '\n';
            return 0;
        }

        if (command == "scanner-read")
        {
            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            std::cout << settings.read(scanner.scannerPath).ToString() << '\n';
            return 0;
        }

        if (command == "scanner-schema")
        {
            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            std::cout << settings.schema(scanner.scannerPath).ToString() << '\n';
            return 0;
        }

        if (command == "params-all")
        {
            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            GoPxLSdk::GoJson allParams;
            allParams.Set("/scanner", settings.readParameters(scanner.scannerPath));
            allParams.Set("/sensor", settings.readParameters(scanner.sensorPath));
            std::cout << allParams.ToString() << '\n';
            return 0;
        }

        if (command == "params-flat")
        {
            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            GoPxLSdk::GoJson allParams;
            allParams.Set("/scanner", settings.readParameters(scanner.scannerPath));
            allParams.Set("/sensor", settings.readParameters(scanner.sensorPath));
            
            allParams.Flatten();
            for (auto it = allParams.Begin(); it != allParams.End(); it++)
            {
                std::cout << it.Key() << " = " << it.Value().ToString() << '\n';
            }
            return 0;
        }

        if (command == "tool-list")
        {
            std::vector<std::string> tools = settings.listTools();
            for (const std::string& toolId : tools)
            {
                std::cout << toolId << '\n';
            }
            return 0;
        }

        if (command == "tool-params")
        {
            if (argc < 4)
            {
                printUsage(argv[0]);
                return 2;
            }
            std::cout << settings.readToolParameters(argv[3]).ToString() << '\n';
            return 0;
        }

        if (command == "config-save")
        {
            if (argc < 4)
            {
                printUsage(argv[0]);
                return 2;
            }
            const std::string filename = argv[3];
            gocator::GocatorParameterSet config = settings.readAllConfig();
            std::ofstream out(filename);
            if (!out)
            {
                throw std::runtime_error("Cannot open file for writing: " + filename);
            }
            out << config.toJson().ToString();
            std::cout << "Config saved to " << filename << '\n';
            return 0;
        }

        if (command == "config-load")
        {
            if (argc < 4)
            {
                printUsage(argv[0]);
                return 2;
            }
            const std::string filename = argv[3];
            std::ifstream in(filename);
            if (!in)
            {
                throw std::runtime_error("Cannot open file for reading: " + filename);
            }
            std::stringstream buffer;
            buffer << in.rdbuf();
            gocator::GocatorParameterSet config(GoPxLSdk::GoJson(buffer.str()));
            settings.applyConfig(config);
            std::cout << "Config loaded from " << filename << " and applied to sensor.\n";
            return 0;
        }

        if (command == "tune")
        {
            if (argc < 6)
            {
                printUsage(argv[0]);
                return 2;
            }

            const gocator::ScannerInfo scanner = settings.detectPrimaryScanner();
            const gocator::ScanTuningOptions options = parseTuning(argc, argv, 3);
            settings.stopIfRunning();
            settings.configureScanTuning(scanner, options);
            std::cout << "configured scanMode=" << options.profileMode.scanMode
                      << " intensity=" << options.profileMode.intensityEnabled
                      << " uniform=" << options.profileMode.uniformSpacingEnabled;
            if (options.updateExposure)
            {
                std::cout << " exposure=" << options.exposure;
            }
            std::cout << '\n';
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
