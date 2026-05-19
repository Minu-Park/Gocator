#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <GoPxLSdk/GoJson.h>

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

    ScannerInfo detectPrimaryScanner();
    void stopIfRunning() noexcept;
    void enableGocatorProtocol(bool enabled);
    void clearGocatorOutputs();
    void addOutput(const std::string& sourceId, int outputId = 0, bool autoShift = true);
    void configureProfileMode(const ScannerInfo& scanner, const ProfileModeOptions& options = {});
    ScannerInfo prepareProfileOutput(const ProfileModeOptions& options = {});

private:
    GoPxLSdk::GoSystem& system();
    const GoPxLSdk::GoSystem& system() const;

    GocatorConnectionConfig config_;
    std::unique_ptr<GoPxLSdk::GoSystem> system_;
};

} // namespace gocator
