#include "gocator/GocatorSettingsManager.h"

#include <stdexcept>
#include <sstream>
#include <utility>

#include <GoPxLSdk/Def.h>
#include <GoPxLSdk/GoSystem.h>
#include <kApi/Io/kNetwork.h>

#include "gocator/GocatorSdkRuntime.h"

namespace gocator
{
namespace
{

constexpr const char* kGocatorControlPath = "/controls/gocator";
constexpr const char* kAddOutputPath = "/controls/gocator/outputs/commands/add";
constexpr const char* kRemoveAllOutputPath = "/controls/gocator/outputs/commands/removeAll";

std::string jsonString(const std::string& value)
{
    std::ostringstream out;
    out << '"';

    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }

    out << '"';
    return out.str();
}

std::string profileSourceForEngine(const std::string& engineId, const std::string& scannerId)
{
    if (engineId == "LMIConfocalLineProfiler")
    {
        return "scan:" + engineId + ":" + scannerId + ":topUniformProfileLayer0";
    }

    return "scan:" + engineId + ":" + scannerId + ":topUniformProfile";
}

} // namespace

GocatorSettingsManager::GocatorSettingsManager(GocatorConnectionConfig config)
    : config_(std::move(config))
{
    GocatorSdkRuntime::ensureInitialized();

    kIpAddress address = {};
    const kStatus parseStatus = kIpAddress_Parse(&address, config_.address.c_str());
    if (parseStatus != kOK)
    {
        throw std::invalid_argument("Invalid Gocator IP address: " + config_.address);
    }

    system_ = std::make_unique<GoPxLSdk::GoSystem>(address, config_.controlPort);
}

GocatorSettingsManager::~GocatorSettingsManager()
{
    disconnect();
}

void GocatorSettingsManager::connect()
{
    system().Connect();
}

void GocatorSettingsManager::disconnect() noexcept
{
    if (!system_)
    {
        return;
    }

    try
    {
        if (system_->IsConnected())
        {
            system_->Disconnect();
        }
    }
    catch (...)
    {
    }
}

bool GocatorSettingsManager::isConnected()
{
    return system().IsConnected();
}

GoPxLSdk::GoJson GocatorSettingsManager::read(const std::string& path)
{
    return system().Client().Read(path).GetResponse().Payload();
}

void GocatorSettingsManager::update(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    system().Client().Update(path, payload).CheckResponse(config_.commandTimeoutMs);
}

void GocatorSettingsManager::call(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    system().Client().Call(path, payload).CheckResponse(config_.commandTimeoutMs);
}

ScannerInfo GocatorSettingsManager::detectPrimaryScanner()
{
    GoPxLSdk::GoJson sensorsRoot = read("/scan/visibleSensors/");
    GoPxLSdk::GoJson sensors = sensorsRoot.At("/sensors");
    if (sensors.Size() == 0)
    {
        throw std::runtime_error("No visible Gocator sensors");
    }

    GoPxLSdk::GoJson sensor = sensors.At("/0");

    ScannerInfo info;
    info.engineId = sensor.At("/engineId").Get<std::string>();
    info.scannerId = "scanner-0";
    info.scannerPath = "/scan/engines/" + info.engineId + "/scanners/" + info.scannerId;
    info.profileSourceId = profileSourceForEngine(info.engineId, info.scannerId);
    info.model = sensor.At("/model").Get<std::string>();
    info.serialNumber = sensor.At("/serialNumber").Get<std::string>();
    return info;
}

void GocatorSettingsManager::stopIfRunning() noexcept
{
    try
    {
        system().Stop();
    }
    catch (...)
    {
    }
}

void GocatorSettingsManager::enableGocatorProtocol(bool enabled)
{
    update(kGocatorControlPath, GoPxLSdk::GoJson(std::string("{\"enabled\":") + (enabled ? "true" : "false") + "}"));
}

void GocatorSettingsManager::clearGocatorOutputs()
{
    call(kRemoveAllOutputPath, GoPxLSdk::GoJson("{}"));
}

void GocatorSettingsManager::addOutput(const std::string& sourceId, int outputId, bool autoShift)
{
    std::ostringstream payload;
    payload << "{"
            << "\"source\":" << jsonString(sourceId) << ","
            << "\"outputId\":" << outputId << ","
            << "\"autoShift\":" << (autoShift ? "true" : "false")
            << "}";

    call(kAddOutputPath, GoPxLSdk::GoJson(payload.str()));
}

void GocatorSettingsManager::configureProfileMode(const ScannerInfo& scanner, const ProfileModeOptions& options)
{
    std::ostringstream payload;
    payload << "{"
            << "\"parameters\":{"
            << "\"scanModeSettings\":{"
            << "\"scanMode\":" << options.scanMode << ","
            << "\"intensityEnabled\":" << (options.intensityEnabled ? "true" : "false") << ","
            << "\"uniformSpacingEnabled\":" << (options.uniformSpacingEnabled ? "true" : "false")
            << "}}}";

    update(scanner.scannerPath, GoPxLSdk::GoJson(payload.str()));
}

ScannerInfo GocatorSettingsManager::prepareProfileOutput(const ProfileModeOptions& options)
{
    stopIfRunning();
    ScannerInfo scanner = detectPrimaryScanner();
    configureProfileMode(scanner, options);
    enableGocatorProtocol(true);
    clearGocatorOutputs();
    addOutput(scanner.profileSourceId);
    return scanner;
}

GoPxLSdk::GoSystem& GocatorSettingsManager::system()
{
    if (!system_)
    {
        throw std::logic_error("Gocator system is not initialized");
    }

    return *system_;
}

const GoPxLSdk::GoSystem& GocatorSettingsManager::system() const
{
    if (!system_)
    {
        throw std::logic_error("Gocator system is not initialized");
    }

    return *system_;
}

} // namespace gocator
