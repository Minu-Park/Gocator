#include "gocator/GocatorSettingsManager.h"

#include <stdexcept>
#include <sstream>
#include <utility>

#include <GoPxLSdk/Def.h>
#include <GoPxLSdk/GoSystem.h>

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
    : connection_(std::move(config))
    , resources_(connection_)
{
}

GocatorSettingsManager::~GocatorSettingsManager()
{
    disconnect();
}

void GocatorSettingsManager::connect()
{
    connection_.connect();
}

void GocatorSettingsManager::disconnect() noexcept
{
    connection_.disconnect();
}

bool GocatorSettingsManager::isConnected()
{
    return connection_.isConnected();
}

GoPxLSdk::GoJson GocatorSettingsManager::read(const std::string& path)
{
    return resources_.read(path);
}

void GocatorSettingsManager::update(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    resources_.update(path, payload);
}

void GocatorSettingsManager::call(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    resources_.call(path, payload);
}

GoPxLSdk::GoJson GocatorSettingsManager::schema(const std::string& path)
{
    return resources_.schema(path);
}

GoPxLSdk::GoJson GocatorSettingsManager::schemaFor(const std::string& path, const std::string& propertyPath)
{
    return resources_.schemaFor(path, propertyPath);
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
    info.sensorPath = info.scannerPath + "/sensors/sensor-0";
    info.profileSourceId = profileSourceForEngine(info.engineId, info.scannerId);
    info.model = sensor.At("/model").Get<std::string>();
    info.serialNumber = sensor.At("/serialNumber").Get<std::string>();
    return info;
}

void GocatorSettingsManager::stopIfRunning() noexcept
{
    connection_.stopNoThrow();
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

void GocatorSettingsManager::configureScanTuning(const ScannerInfo& scanner, const ScanTuningOptions& options)
{
    std::ostringstream payload;
    payload << "{"
            << "\"parameters\":{";

    payload << "\"scanModeSettings\":{"
            << "\"scanMode\":" << options.profileMode.scanMode << ","
            << "\"intensityEnabled\":" << (options.profileMode.intensityEnabled ? "true" : "false") << ","
            << "\"uniformSpacingEnabled\":" << (options.profileMode.uniformSpacingEnabled ? "true" : "false")
            << "}}}";

    update(scanner.scannerPath, GoPxLSdk::GoJson(payload.str()));

    if (options.updateExposure)
    {
        std::ostringstream sensorPayload;
        sensorPayload << "{"
                      << "\"parameters\":{"
                      << "\"exposureSettings\":{"
                      << "\"exposureMode\":0,"
                      << "\"singleExposure\":" << options.exposure
                      << "}}}";

        update(scanner.sensorPath, GoPxLSdk::GoJson(sensorPayload.str()));
    }

    if (options.updateActiveArea)
    {
        std::ostringstream sensorPayload;
        sensorPayload << "{"
                      << "\"parameters\":{"
                      << "\"activeAreaSettings\":{"
                      << "\"activeArea\":{"
                      << "\"z\":" << options.activeAreaZ << ","
                      << "\"height\":" << options.activeAreaHeight
                      << "}}}}";

        update(scanner.sensorPath, GoPxLSdk::GoJson(sensorPayload.str()));
    }

    if (options.updateFlexSpotThreshold)
    {
        std::ostringstream sensorPayload;
        sensorPayload << "{"
                      << "\"parameters\":{"
                      << "\"advancedSettings\":{"
                      << "\"materialType\":0,"
                      << "\"spotDetection\":{"
                      << "\"flexSpots\":{"
                      << "\"threshold\":" << options.flexSpotThreshold
                      << "}}}}}";

        update(scanner.sensorPath, GoPxLSdk::GoJson(sensorPayload.str()));
    }
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
    return connection_.system();
}

const GoPxLSdk::GoSystem& GocatorSettingsManager::system() const
{
    return connection_.system();
}

} // namespace gocator
