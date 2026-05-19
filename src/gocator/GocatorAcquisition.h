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
    std::vector<std::uint8_t> pixels;
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

struct GocatorFrame
{
    std::size_t messageCount = 0;
    std::vector<GocatorFrameMessage> messages;
    std::vector<GocatorImageFrame> images;
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

private:
    static GocatorFrame frameFromDataSet(const GoPxLSdk::GoDataSet& dataSet);
    static GocatorFrameMessage messageInfo(const GoPxLSdk::GoGdpMsg& message);
    static GocatorImageFrame imageFrame(const GoPxLSdk::GoGdpImage& image);

    GocatorConnection connection_;
    std::unique_ptr<GoPxLSdk::GoGdpClient> gdpClient_;
};

} // namespace gocator
