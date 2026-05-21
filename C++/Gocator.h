#ifndef GOCATOR_H
#define GOCATOR_H

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace GoPxLSdk
{
class GoDataSet;
}

class Gocator
{
public:
    using CallbackId = size_t;

    Gocator();
    ~Gocator();

    enum Status {
        GrabbingStatus,
        ConnectionStatus
    };

    struct DeviceInfo {
        std::string address;
        std::string model;
        std::string serial;
    };

    enum ScanMode {
        ProfileMode = 0,
        SurfaceMode = 1
    };

    using StatusCallback = std::function<void(Status status, bool on)>;
    CallbackId registerStatusCallback(StatusCallback cb);
    bool deregisterStatusCallback(CallbackId id);
    void clearStatusCallbacks();

    using GrabCallback = std::function<void(const GoPxLSdk::GoDataSet& dataSet, size_t frameSeq)>;
    CallbackId registerGrabCallback(GrabCallback cb);
    bool deregisterGrabCallback(CallbackId id);
    void clearGrabCallbacks();

    std::vector<DeviceInfo> discoverDevices();

    bool open(const std::string& ipAddress);
    bool isOpened() const;
    void close();

    void configure(double scanLengthMm, ScanMode mode, bool intensityEnabled, bool uniformSpacingEnabled, int exposureUs);

    std::string getParametersSchema(const std::string& type) const;
    std::string getParametersData(const std::string& type) const;
    void setParameterValue(const std::string& type, const std::string& path, const std::string& jsonValue);

    void grab(size_t frames = 0);
    void stop();
    bool isGrabbing() const;

    std::string getConnectedAddress() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // GOCATOR_H
