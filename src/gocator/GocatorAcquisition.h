#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gocator/GocatorConnection.h"
#include "gocator/GocatorTypes.h"

namespace GoPxLSdk
{
class GoGdpClient;
class GoDataSet;
class GoGdpImage;
class GoGdpMsg;
class GoGdpSpots;
}

namespace gocator
{

struct GocatorImageFrame
{
    std::string sourceId;
    std::uint64_t dataSetId = 0;
    std::uint16_t gdpId = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t pixelSize = 0;
    std::int32_t pixelFormat = 0;
    std::string pixelFormatName;
    std::size_t dataSize = 0;
    bool hasByteStats = false;
    std::uint8_t minByte = 0;
    std::uint8_t maxByte = 0;
    double meanByte = 0.0;
    std::vector<std::uint8_t> pixels;
};

struct GocatorProfilePoint
{
    double x = 0.0;
    double z = 0.0;
    std::uint16_t intensity = 0;
    bool valid = false;
};

struct GocatorProfileFrame
{
    std::string sourceId;
    std::uint64_t dataSetId = 0;
    std::uint16_t gdpId = 0;
    std::uint32_t width = 0;
    std::uint32_t intensityWidth = 0;
    double xResolution = 0.0;
    double zResolution = 0.0;
    double xOffset = 0.0;
    double zOffset = 0.0;
    std::size_t validCount = 0;
    std::size_t nullCount = 0;
    bool hasRangeStats = false;
    std::int32_t minRange = 0;
    std::int32_t maxRange = 0;
    std::int32_t firstRange = 0;
    bool hasIntensityStats = false;
    std::uint16_t minIntensity = 0;
    std::uint16_t maxIntensity = 0;
    std::vector<GocatorProfilePoint> points;
};

struct GocatorFrameMessage
{
    std::string typeName;
    std::uint16_t typeValue = 0;
    std::string sourceId;
    std::uint64_t dataSetId = 0;
    std::uint16_t gdpId = 0;
    bool isLastMessage = false;
};

struct GocatorSpotsFrame
{
    std::string sourceId;
    std::uint64_t dataSetId = 0;
    std::uint16_t gdpId = 0;
    std::uint32_t pointCount = 0;
    float exposure = 0.0F;
    bool columnBased = false;
    std::uint32_t maxSliceCount = 0;
    std::uint32_t spotCenterMin = 0;
    std::uint32_t spotCenterMax = 0;
};

struct GocatorFrame
{
    std::size_t messageCount = 0;
    std::vector<GocatorFrameMessage> messages;
    std::vector<GocatorImageFrame> images;
    std::vector<GocatorProfileFrame> profiles;
    std::vector<GocatorSpotsFrame> spots;
};

class GocatorAcquisition
{
public:
    explicit GocatorAcquisition(GocatorConnectionConfig config);
    ~GocatorAcquisition();

    GocatorAcquisition(const GocatorAcquisition&) = delete;
    GocatorAcquisition& operator=(const GocatorAcquisition&) = delete;

    void connect();
    void disconnect() noexcept;
    bool isConnected();

    void start();
    void stop();
    void stopNoThrow() noexcept;
    void clear();

    GocatorFrame receiveOne(int timeoutMs);
    GocatorFrame grabOne(int timeoutMs, bool stopAfterReceive = true);
    GocatorFrame grabUntilValidProfile(int timeoutMs, int maxFrames, bool stopAfterReceive = true);

private:
    static GocatorFrame frameFromDataSet(const GoPxLSdk::GoDataSet& dataSet);
    static GocatorFrameMessage messageInfo(const GoPxLSdk::GoGdpMsg& message);
    static GocatorImageFrame imageFrame(const GoPxLSdk::GoGdpImage& image);
    static GocatorProfileFrame uniformProfileFrame(const GoPxLSdk::GoGdpMsg& message);
    static GocatorProfileFrame pointCloudProfileFrame(const GoPxLSdk::GoGdpMsg& message);
    static GocatorSpotsFrame spotsFrame(const GoPxLSdk::GoGdpSpots& spots);

    GocatorConnection connection_;
    std::unique_ptr<GoPxLSdk::GoGdpClient> gdpClient_;
};

} // namespace gocator
