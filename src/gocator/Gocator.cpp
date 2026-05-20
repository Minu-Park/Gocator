#include "gocator/Gocator.h"

#include <GoPxLSdk/GoDataSet.h>
#include <GoPxLSdk/GoDiscoveryClient.h>
#include <GoPxLSdk/GoGdpClient.h>
#include <GoPxLSdk/GoInstance.h>
#include <GoPxLSdk/GoJson.h>
#include <GoPxLSdk/GoResource.h>
#include <GoPxLSdk/GoSystem.h>
#include <kApi/Io/kNetwork.h>
#include <kApi/kApiDef.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <iostream>

#include "gocator/GocatorDiscovery.h"
#include "gocator/GocatorSdkRuntime.h"

namespace
{
constexpr const char* GOCATOR_CONTROL_PATH = "/controls/gocator";
constexpr const char* GOCATOR_ADD_OUTPUT_PATH = "/controls/gocator/outputs/commands/add";
constexpr const char* GOCATOR_REMOVE_ALL_OUTPUT_PATH = "/controls/gocator/outputs/commands/removeAll";

struct GocatorTarget
{
    kIpAddress address{};
    k16u controlPort = GO_PXL_SDK_DEFAULT_CONTROL_PORT;
};

[[nodiscard]] GocatorTarget parseTarget(const std::string& ipAddress)
{
    GocatorTarget target;
    if (kIpAddress_Parse(&target.address, ipAddress.c_str()) != kOK)
    {
        throw std::runtime_error("invalid Gocator IP address: " + ipAddress);
    }
    return target;
}

[[nodiscard]] std::string ipAddressToString(const kIpAddress& address)
{
    kChar text[64] = {};
    kIpAddress_Format(address, text, sizeof(text));
    return text;
}

[[nodiscard]] GocatorTarget discoverTarget(int timeoutMs)
{
    GoPxLSdk::GoDiscoveryClient discovery;
    discovery.BlockingDiscover(static_cast<k64u>(timeoutMs), false);
    const std::vector<GoPxLSdk::GoInstance>& instances = discovery.InstanceList();
    for (const GoPxLSdk::GoInstance& instance : instances)
    {
        if (!instance.GetIsRemote())
        {
            return {instance.GetIpAddress(), instance.GetControlPort()};
        }
    }
    throw std::runtime_error("no local Gocator/GoPxL instance discovered");
}

[[nodiscard]] std::string detectEngineId(GoPxLSdk::GoSystem& system)
{
    try
    {
        GoPxLSdk::GoJson sensorsResponse = system.Client().Read("/scan/visibleSensors/").GetResponse().Payload();
        GoPxLSdk::GoJson sensors = sensorsResponse.At("/sensors");
        if (sensors.Size() > 0U)
        {
            return sensors.At("/0/engineId").Get<std::string>();
        }
    }
    catch (...) {}
    return "LMIConfocalLineProfiler";
}

void enableGocatorProtocol(GoPxLSdk::GoSystem& system, int timeoutMs)
{
    system.Client()
        .Update(GOCATOR_CONTROL_PATH, GoPxLSdk::GoJson(R"({"enabled": true})"))
        .CheckResponse(static_cast<k64u>(timeoutMs));
}

} // namespace

struct Gocator::Impl
{
    std::string ipAddress;
    std::unique_ptr<GoPxLSdk::GoSystem> system;
    std::unique_ptr<GoPxLSdk::GoGdpClient> gdpClient;
    std::atomic<bool> isOpened{false};
    std::atomic<bool> isGrabbing{false};
    std::atomic<bool> stopRequested{false};

    std::string getScannerPath() const
    {
        if (!system) return "";
        const std::string engineId = detectEngineId(*system);
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
            gocator::GocatorSdkRuntime::ensureInitialized();

            GocatorTarget target;
            if (!ip.empty())
            {
                target = parseTarget(ip);
            }
            else
            {
                target = discoverTarget(3000); // 3 seconds timeout
            }

            system = std::make_unique<GoPxLSdk::GoSystem>(target.address, target.controlPort);
            system->Connect();

            ipAddress = ip.empty() ? ipAddressToString(target.address) : ip;
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

        if (system)
        {
            try { system->Disconnect(); } catch (...) {}
            system.reset();
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
            const std::string engineId = detectEngineId(*system);
            const std::string scannerPath = "/scan/engines/" + engineId + "/scanners/scanner-0";
            const std::string sensorPath = scannerPath + "/sensors/sensor-0";

            // 1. Scan Mode, Intensity, Uniform Spacing 적용
            int sdkScanMode = 2; // Default Profile
            if (mode == ScanMode::SurfaceMode)
            {
                sdkScanMode = 3; // Surface
            }

            std::string payload = R"({"parameters":{"scanModeSettings":{)"
                                  R"("scanMode":)" + std::to_string(sdkScanMode) + R"(,)"
                                  R"("intensityEnabled":)" + (intensityEnabled ? "true" : "false") + R"(,)"
                                  R"("uniformSpacingEnabled":)" + (uniformSpacingEnabled ? "true" : "false") +
                                  R"(}}})";
            system->Client().Update(scannerPath, GoPxLSdk::GoJson(payload)).CheckResponse(30000);

            // 2. Exposure 적용
            std::string exposurePayload = R"({"parameters":{"exposureSettings":{)"
                                          R"("exposureMode":0,)"
                                          R"("singleExposure":)" + std::to_string(exposureUs) +
                                          R"(}}})";
            system->Client().Update(sensorPath, GoPxLSdk::GoJson(exposurePayload)).CheckResponse(30000);

            // 3. Scan Length 적용
            std::string scanLengthPayload = R"({"parameters":{"scanModeSettings":{"scanLengthMm":)" + std::to_string(scanLengthMm) + R"(}}})";
            system->Client().Update(scannerPath, GoPxLSdk::GoJson(scanLengthPayload)).CheckResponse(30000);

            // 4. Gocator protocol 활성화 및 Output 구성
            enableGocatorProtocol(*system, 30000);

            // Remove outputs
            try
            {
                system->Client().Call(GOCATOR_REMOVE_ALL_OUTPUT_PATH, GoPxLSdk::GoJson("{}")).CheckResponse(30000);
            }
            catch (...) {}

            // Add output
            std::string sourceId;
            if (mode == ScanMode::SurfaceMode)
            {
                sourceId = (engineId == "LMIConfocalLineProfiler")
                    ? "scan:" + engineId + ":scanner-0:topUniformSurfaceLayer0"
                    : "scan:" + engineId + ":scanner-0:topUniformSurface";
            }
            else
            {
                sourceId = (engineId == "LMIConfocalLineProfiler")
                    ? "scan:" + engineId + ":scanner-0:topUniformProfileLayer0"
                    : "scan:" + engineId + ":scanner-0:topUniformProfile";
            }

            std::string addOutputPayload = R"({"source":")" + sourceId + R"(","outputId":0,"autoShift":true})";
            system->Client().Call(GOCATOR_ADD_OUTPUT_PATH, GoPxLSdk::GoJson(addOutputPayload)).CheckResponse(30000);
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
            gdpClient->Connect(system->Address(), system->GdpPort());

            frameSeq = 0;
            frameTarget = frames;
            stopRequested.store(false);
            isGrabbing.store(true);

            dataCallback = [this](const GoPxLSdk::GoDataSet& dataSet) {
                handleData(dataSet);
            };
            gdpClient->ReceiveDataAsync(dataCallback);

            system->Start();
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
        joinStopWorker();

        if (!isGrabbing.load()) return;

        stopRequested.store(true);
        isGrabbing.store(false);

        if (system)
        {
            try { system->Stop(); } catch (...) {}
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
        std::vector<gocator::GocatorDeviceInfo> devices = discovery.discover();
        result.reserve(devices.size());
        for (const auto& d : devices)
        {
            result.push_back({d.address, d.deviceModel, std::to_string(d.serialNumber)});
        }
    }
    catch (...)
    {
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
        return _impl->system->Resource(path)->Schema().ToString();
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
        return _impl->system->Resource(path)->Data().ToString();
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
        auto resource = _impl->system->Resource(resPath);
        GoPxLSdk::GoJson patch;
        patch.Set(path, GoPxLSdk::GoJson(jsonValue));
        resource->SetJson(patch);
    }
    catch (const std::exception& e)
    {
        std::cerr << "setParameterValue failed: " << e.what() << " (path: " << path << ", val: " << jsonValue << ")" << std::endl;
    }
}
