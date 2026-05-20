#include "gocator/GocatorSettingsManager.h"

#include <stdexcept>
#include <sstream>
#include <utility>

#include <GoPxLSdk/Def.h>
#include <GoPxLSdk/GoSystem.h>

#include "gocator/GocatorParameterSet.h"

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

ScannerInfo GocatorSettingsManager::prepareSurfaceOutput(const ProfileModeOptions& options)
{
    stopIfRunning();
    ScannerInfo scanner = detectPrimaryScanner();
    configureProfileMode(scanner, options);
    enableGocatorProtocol(true);
    clearGocatorOutputs();
    
    std::string surfaceSource = scanner.profileSourceId;
    // Try to find a surface source
    size_t profilePos = surfaceSource.find("Profile");
    if (profilePos != std::string::npos)
    {
        surfaceSource.replace(profilePos, 7, "Surface");
    }
    
    addOutput(surfaceSource);
    return scanner;
}

std::vector<std::string> GocatorSettingsManager::listSources(const std::string& scannerPath)
{
    std::vector<std::string> sources;
    try
    {
        GoPxLSdk::GoJson data = read(scannerPath + "/sources");
        if (data.IsArray())
        {
            for (std::size_t i = 0; i < data.Size(); ++i)
            {
                sources.push_back(data.At("/" + std::to_string(i)).Get<std::string>());
            }
        }
    }
    catch (...)
    {
        // Fallback or empty
    }
    return sources;
}

GoPxLSdk::GoJson GocatorSettingsManager::readParameters(const std::string& path)
{
    GoPxLSdk::GoJson resource = read(path);
    if (resource.HasKey("parameters"))
    {
        return resource.At("/parameters");
    }
    return GoPxLSdk::GoJson("{}");
}

void GocatorSettingsManager::updateParameters(const std::string& path, const GoPxLSdk::GoJson& parameters)
{
    GoPxLSdk::GoJson payload;
    payload.Set("/parameters", parameters);
    update(path, payload);
}

std::vector<std::string> GocatorSettingsManager::listTools()
{
    std::vector<std::string> tools;
    try
    {
        GoPxLSdk::GoJson data = read("/tools");
        if (data.HasKey("_embedded") && data.At("/_embedded").HasKey("go:item"))
        {
            GoPxLSdk::GoJson items = data.At("/_embedded/go:item");
            for (std::size_t i = 0; i < items.Size(); ++i)
            {
                tools.push_back(items.At("/" + std::to_string(i) + "/id").Get<std::string>());
            }
        }
    }
    catch (...)
    {
    }
    return tools;
}

GoPxLSdk::GoJson GocatorSettingsManager::readTool(const std::string& toolId)
{
    return read("/tools/" + toolId);
}

GoPxLSdk::GoJson GocatorSettingsManager::readToolParameters(const std::string& toolId)
{
    return readParameters("/tools/" + toolId);
}

GocatorParameterSet GocatorSettingsManager::readAllConfig()
{
    GocatorParameterSet config;
    ScannerInfo scanner = detectPrimaryScanner();

    config.addParameter("scanner", readParameters(scanner.scannerPath));
    config.addParameter("sensor", readParameters(scanner.sensorPath));

    GoPxLSdk::GoJson tools;
    std::vector<std::string> toolIds = listTools();
    for (const std::string& id : toolIds)
    {
        tools.Set(id, readToolParameters(id));
    }
    config.addParameter("tools", tools);

    return config;
}

void GocatorSettingsManager::applyConfig(const GocatorParameterSet& config)
{
    ScannerInfo scanner = detectPrimaryScanner();

    if (config.hasParameter("scanner"))
    {
        updateParameters(scanner.scannerPath, config.getParameter("scanner"));
    }

    if (config.hasParameter("sensor"))
    {
        updateParameters(scanner.sensorPath, config.getParameter("sensor"));
    }

    if (config.hasParameter("tools"))
    {
        GoPxLSdk::GoJson tools = config.getParameter("tools");
        // For tools, we might want to match by ID.
        // Simplified: just update existing tools if they match.
        for (auto it = tools.Begin(); it != tools.End(); it++)
        {
            try
            {
                updateParameters("/tools/" + it.Key(), it.Value());
            }
            catch (...)
            {
                // Tool might not exist on the target sensor
            }
        }
    }
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
