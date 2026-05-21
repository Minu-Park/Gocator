#include "Gocator.h"

#include <GoPxLSdk/GoDataSet.h>
#include <GoPxLSdk/GoGdpClient.h>
#include <GoPxLSdk/GoJson.h>
#include <GoPxLSdk/GoSystem.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Internal/GocatorConnection.h"
#include "Internal/GocatorDiscovery.h"
#include "Internal/GocatorResourceClient.h"

namespace
{
constexpr const char* GOCATOR_CONTROL_PATH = "/controls/gocator";
constexpr const char* GOCATOR_ADD_OUTPUT_PATH = "/controls/gocator/outputs/commands/add";
constexpr const char* GOCATOR_REMOVE_ALL_OUTPUT_PATH = "/controls/gocator/outputs/commands/removeAll";

[[nodiscard]] gocator::GocatorConnectionConfig resolveTarget(const std::string& ipAddress)
{
    if (!ipAddress.empty())
    {
        return gocator::GocatorDiscovery::manualTarget(ipAddress);
    }

    const gocator::GocatorDiscovery discovery;
    const std::vector<gocator::GocatorDeviceInfo> devices = discovery.discover({3000, false});
    for (const gocator::GocatorDeviceInfo& device : devices)
    {
        if (device.canConnectLocally())
        {
            return device.connectionConfig();
        }
    }

    throw std::runtime_error("no local Gocator/GoPxL instance discovered");
}

[[nodiscard]] std::string detectEngineId(gocator::GocatorResourceClient& resources)
{
    try
    {
        GoPxLSdk::GoJson sensorsResponse = resources.read("/scan/visibleSensors/");
        GoPxLSdk::GoJson sensors = sensorsResponse.At("/sensors");
        if (sensors.Size() > 0U)
        {
            return sensors.At("/0/engineId").Get<std::string>();
        }
    }
    catch (...) {}
    return "LMIConfocalLineProfiler";
}

[[nodiscard]] std::string profileSourceForEngine(const std::string& engineId, Gocator::ScanMode mode)
{
    if (mode == Gocator::ScanMode::SurfaceMode)
    {
        return (engineId == "LMIConfocalLineProfiler")
            ? "scan:" + engineId + ":scanner-0:topUniformSurfaceLayer0"
            : "scan:" + engineId + ":scanner-0:topUniformSurface";
    }

    return (engineId == "LMIConfocalLineProfiler")
        ? "scan:" + engineId + ":scanner-0:topUniformProfileLayer0"
        : "scan:" + engineId + ":scanner-0:topUniformProfile";
}

} // namespace

struct Gocator::Impl
{
    std::string ipAddress;
    std::unique_ptr<gocator::GocatorConnection> connection;
    std::unique_ptr<gocator::GocatorResourceClient> resources;
    std::unique_ptr<GoPxLSdk::GoGdpClient> gdpClient;
    std::atomic<bool> isOpened{false};
    std::atomic<bool> isGrabbing{false};
    std::atomic<bool> stopRequested{false};

    std::string getScannerPath() const
    {
        if (!resources) return "";
        const std::string engineId = detectEngineId(*resources);
        return "/scan/engines/" + engineId + "/scanners/scanner-0";
    }

    std::string getSensorPath() const
    {
        return getScannerPath() + "/sensors/sensor-0";
    }

    std::mutex statusMutex;
    std::unordered_map<size_t, StatusCallback> statusObservers;
    std::atomic<size_t> nextStatusObserverId{1};

    std::mutex grabCallbackMutex;
    std::unordered_map<size_t, GrabCallback> grabCallbacks;
    std::atomic<size_t> nextGrabCallbackId{1};

    size_t frameSeq = 0;
    size_t frameTarget = 0;
    std::function<void(const GoPxLSdk::GoDataSet&)> dataCallback;
    std::mutex stopWorkerMutex;
    std::thread stopWorker;
    std::mutex stopMutex;

    ~Impl()
    {
        close();
        joinStopWorker();
    }

    void notifyStatus(Status status, bool on)
    {
        std::vector<StatusCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            callbacks.reserve(statusObservers.size());
            for (const auto& [id, cb] : statusObservers)
            {
                callbacks.push_back(cb);
            }
        }

        for (const auto& cb : callbacks)
        {
            if (cb)
            {
                cb(status, on);
            }
        }
    }

    bool open(const std::string& ip)
    {
        if (isOpened.load()) return true;

        try
        {
            auto target = resolveTarget(ip);
            connection = std::make_unique<gocator::GocatorConnection>(target);
            resources = std::make_unique<gocator::GocatorResourceClient>(*connection);
            connection->connect();

            ipAddress = connection->config().address;
            isOpened.store(true);
            notifyStatus(ConnectionStatus, true);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Gocator open failed: " << e.what() << std::endl;
            close();
            return false;
        }
    }

    void close()
    {
        joinStopWorker();
        stop();

        if (gdpClient)
        {
            try { gdpClient->Close(); } catch (...) {}
            gdpClient.reset();
        }

        resources.reset();
        if (connection)
        {
            connection->disconnect();
            connection.reset();
        }

        if (isOpened.load())
        {
            isOpened.store(false);
            notifyStatus(ConnectionStatus, false);
        }
    }

    void configure(double scanLengthMm, ScanMode mode, bool intensityEnabled, bool uniformSpacingEnabled, int exposureUs)
    {
        if (!isOpened.load()) return;

        try
        {
            const std::string engineId = detectEngineId(*resources);
            const std::string scannerPath = "/scan/engines/" + engineId + "/scanners/scanner-0";
            const std::string sensorPath = scannerPath + "/sensors/sensor-0";

            int sdkScanMode = 2;
            if (mode == ScanMode::SurfaceMode)
            {
                sdkScanMode = 3;
            }

            std::string payload = R"({"parameters":{"scanModeSettings":{)"
                                  R"("scanMode":)" + std::to_string(sdkScanMode) + R"(,)"
                                  R"("intensityEnabled":)" + (intensityEnabled ? "true" : "false") + R"(,)"
                                  R"("uniformSpacingEnabled":)" + (uniformSpacingEnabled ? "true" : "false") +
                                  R"(}}})";
            resources->update(scannerPath, GoPxLSdk::GoJson(payload));

            std::string exposurePayload = R"({"parameters":{"exposureSettings":{)"
                                          R"("exposureMode":0,)"
                                          R"("singleExposure":)" + std::to_string(exposureUs) +
                                          R"(}}})";
            resources->update(sensorPath, GoPxLSdk::GoJson(exposurePayload));

            std::string scanLengthPayload = R"({"parameters":{"scanModeSettings":{"scanLengthMm":)" + std::to_string(scanLengthMm) + R"(}}})";
            resources->update(scannerPath, GoPxLSdk::GoJson(scanLengthPayload));

            resources->update(GOCATOR_CONTROL_PATH, GoPxLSdk::GoJson(R"({"enabled": true})"));

            try
            {
                resources->call(GOCATOR_REMOVE_ALL_OUTPUT_PATH, GoPxLSdk::GoJson("{}"));
            }
            catch (...) {}

            const std::string sourceId = profileSourceForEngine(engineId, mode);
            std::string addOutputPayload = R"({"source":")" + sourceId + R"(","outputId":0,"autoShift":true})";
            resources->call(GOCATOR_ADD_OUTPUT_PATH, GoPxLSdk::GoJson(addOutputPayload));
        }
        catch (const std::exception& e)
        {
            std::cerr << "Gocator configure failed: " << e.what() << std::endl;
        }
    }

    void grab(size_t frames)
    {
        joinStopWorker();

        if (!isOpened.load() || isGrabbing.load()) return;

        try
        {
            gdpClient = std::make_unique<GoPxLSdk::GoGdpClient>();
            gdpClient->Connect(connection->system().Address(), connection->gdpPort());

            frameSeq = 0;
            frameTarget = frames;
            stopRequested.store(false);
            isGrabbing.store(true);

            dataCallback = [this](const GoPxLSdk::GoDataSet& dataSet) {
                handleData(dataSet);
            };
            gdpClient->ReceiveDataAsync(dataCallback);

            connection->start();
            notifyStatus(GrabbingStatus, true);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Gocator grab failed: " << e.what() << std::endl;
            stop();
        }
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(stopMutex);
        if (!isGrabbing.exchange(false)) return;

        stopRequested.store(true);

        if (connection)
        {
            connection->stopNoThrow();
        }

        if (gdpClient)
        {
            try { gdpClient->Close(); } catch (...) {}
            gdpClient.reset();
        }

        dataCallback = {};
        notifyStatus(GrabbingStatus, false);
    }

    void handleData(const GoPxLSdk::GoDataSet& dataSet)
    {
        if (!isGrabbing.load()) return;

        {
            std::vector<GrabCallback> callbacks;
            {
                std::lock_guard<std::mutex> lock(grabCallbackMutex);
                callbacks.reserve(grabCallbacks.size());
                for (const auto& [id, cb] : grabCallbacks)
                {
                    callbacks.push_back(cb);
                }
            }

            for (const auto& cb : callbacks)
            {
                if (cb)
                {
                    cb(dataSet, frameSeq);
                }
            }
        }

        frameSeq++;
        if (frameTarget > 0 && frameSeq >= frameTarget)
        {
            requestStopFromCallback();
        }
    }

    void requestStopFromCallback()
    {
        if (stopRequested.exchange(true))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(stopWorkerMutex);
        if (stopWorker.joinable())
        {
            return;
        }

        stopWorker = std::thread([this]() {
            stop();
        });
    }

    void joinStopWorker()
    {
        std::thread worker;
        {
            std::lock_guard<std::mutex> lock(stopWorkerMutex);
            if (!stopWorker.joinable() || stopWorker.get_id() == std::this_thread::get_id())
            {
                return;
            }
            worker = std::move(stopWorker);
        }

        worker.join();
    }
};

Gocator::Gocator()
    : _impl(std::make_unique<Impl>())
{}

Gocator::~Gocator() = default;

Gocator::CallbackId Gocator::registerStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::mutex> lock(_impl->statusMutex);
    CallbackId id = _impl->nextStatusObserverId++;
    _impl->statusObservers[id] = cb;
    return id;
}

bool Gocator::deregisterStatusCallback(CallbackId id)
{
    std::lock_guard<std::mutex> lock(_impl->statusMutex);
    return _impl->statusObservers.erase(id) > 0;
}

void Gocator::clearStatusCallbacks()
{
    std::lock_guard<std::mutex> lock(_impl->statusMutex);
    _impl->statusObservers.clear();
}

Gocator::CallbackId Gocator::registerGrabCallback(GrabCallback cb)
{
    std::lock_guard<std::mutex> lock(_impl->grabCallbackMutex);
    CallbackId id = _impl->nextGrabCallbackId++;
    _impl->grabCallbacks[id] = cb;
    return id;
}

bool Gocator::deregisterGrabCallback(CallbackId id)
{
    std::lock_guard<std::mutex> lock(_impl->grabCallbackMutex);
    return _impl->grabCallbacks.erase(id) > 0;
}

void Gocator::clearGrabCallbacks()
{
    std::lock_guard<std::mutex> lock(_impl->grabCallbackMutex);
    _impl->grabCallbacks.clear();
}

bool Gocator::open(const std::string& ipAddress)
{
    return _impl->open(ipAddress);
}

bool Gocator::isOpened() const
{
    return _impl->isOpened.load();
}

void Gocator::close()
{
    _impl->close();
}

std::vector<Gocator::DeviceInfo> Gocator::discoverDevices()
{
    std::vector<Gocator::DeviceInfo> result;
    try
    {
        gocator::GocatorDiscovery discovery;
        
        // Try standard discovery first
        std::vector<gocator::GocatorDeviceInfo> devices = discovery.discover({3000, false});
        
        // If nothing found, try classic discovery for older G2/G3 sensors
        if (devices.empty())
        {
            devices = discovery.discover({2000, true});
        }

        result.reserve(devices.size());
        for (const auto& d : devices)
        {
            result.push_back({d.address, d.deviceModel, std::to_string(d.serialNumber)});
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Discovery failed: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Discovery failed with unknown error" << std::endl;
    }
    return result;
}

void Gocator::configure(double scanLengthMm, ScanMode mode, bool intensityEnabled, bool uniformSpacingEnabled, int exposureUs)
{
    _impl->configure(scanLengthMm, mode, intensityEnabled, uniformSpacingEnabled, exposureUs);
}

void Gocator::grab(size_t frames)
{
    _impl->grab(frames);
}

void Gocator::stop()
{
    _impl->stop();
}

bool Gocator::isGrabbing() const
{
    return _impl->isGrabbing.load();
}

std::string Gocator::getConnectedAddress() const
{
    return _impl->ipAddress;
}

std::string Gocator::getParametersSchema(const std::string& type) const
{
    if (!_impl->isOpened.load()) return "{}";
    try
    {
        std::string path = (type == "scanner") ? _impl->getScannerPath() : _impl->getSensorPath();
        return _impl->resources->schema(path).ToString();
    }
    catch (const std::exception& e)
    {
        std::cerr << "getParametersSchema failed: " << e.what() << std::endl;
    }
    return "{}";
}

std::string Gocator::getParametersData(const std::string& type) const
{
    if (!_impl->isOpened.load()) return "{}";
    try
    {
        std::string path = (type == "scanner") ? _impl->getScannerPath() : _impl->getSensorPath();
        return _impl->resources->data(path).ToString();
    }
    catch (const std::exception& e)
    {
        std::cerr << "getParametersData failed: " << e.what() << std::endl;
    }
    return "{}";
}

void Gocator::setParameterValue(const std::string& type, const std::string& path, const std::string& jsonValue)
{
    if (!_impl->isOpened.load()) return;
    try
    {
        std::string resPath = (type == "scanner") ? _impl->getScannerPath() : _impl->getSensorPath();
        GoPxLSdk::GoJson patch;
        patch.Set(path, GoPxLSdk::GoJson(jsonValue));
        _impl->resources->setJson(resPath, patch);
    }
    catch (const std::exception& e)
    {
        std::cerr << "setParameterValue failed: " << e.what() << " (path: " << path << ", val: " << jsonValue << ")" << std::endl;
    }
}
