#pragma once

#include <cstdint>
#include <string>

#include <GoPxLSdk/GoJson.h>

#include "gocator/GocatorConnection.h"
#include "gocator/GocatorResourceClient.h"
#include "gocator/GocatorTypes.h"

namespace GoPxLSdk
{
class GoSystem;
}

namespace gocator
{

struct ScannerInfo
{
    std::string engineId;
    std::string scannerId;
    std::string scannerPath;
    std::string sensorPath;
    std::string profileSourceId;
    std::string model;
    std::string serialNumber;
};

struct ProfileModeOptions
{
    int scanMode = 2;
    bool intensityEnabled = true;
    bool uniformSpacingEnabled = true;
};

struct ScanTuningOptions
{
    ProfileModeOptions profileMode;
    bool updateExposure = false;
    int exposure = 1000;
    bool updateActiveArea = false;
    double activeAreaZ = 0.0;
    double activeAreaHeight = 1.1;
    bool updateFlexSpotThreshold = false;
    int flexSpotThreshold = 12;
};

class GocatorSettingsManager
{
public:
    explicit GocatorSettingsManager(GocatorConnectionConfig config);
    ~GocatorSettingsManager();

    GocatorSettingsManager(const GocatorSettingsManager&) = delete;
    GocatorSettingsManager& operator=(const GocatorSettingsManager&) = delete;

    void connect();
    void disconnect() noexcept;
    bool isConnected();

    GoPxLSdk::GoJson read(const std::string& path);
    void update(const std::string& path, const GoPxLSdk::GoJson& payload);
    void call(const std::string& path, const GoPxLSdk::GoJson& payload);
    GoPxLSdk::GoJson schema(const std::string& path);
    GoPxLSdk::GoJson schemaFor(const std::string& path, const std::string& propertyPath);

    ScannerInfo detectPrimaryScanner();
    void stopIfRunning() noexcept;
    void enableGocatorProtocol(bool enabled);
    void clearGocatorOutputs();
    void addOutput(const std::string& sourceId, int outputId = 0, bool autoShift = true);
    void configureProfileMode(const ScannerInfo& scanner, const ProfileModeOptions& options = {});
    void configureScanTuning(const ScannerInfo& scanner, const ScanTuningOptions& options);
    ScannerInfo prepareProfileOutput(const ProfileModeOptions& options = {});
    ScannerInfo prepareSurfaceOutput(const ProfileModeOptions& options = {});
    std::vector<std::string> listSources(const std::string& scannerPath);

private:
    GoPxLSdk::GoSystem& system();
    const GoPxLSdk::GoSystem& system() const;

    GocatorConnection connection_;
    GocatorResourceClient resources_;
};

} // namespace gocator
