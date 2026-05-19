#include "gocator/GocatorAcquisition.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <GoPxLSdk/GoDataSet.h>
#include <GoPxLSdk/GoGdpClient.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpImage.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpPixelFormat.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpProfilePointCloud.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpProfileUniform.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpSurfaceUniform.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpSpots.h>
#include <GoPxLSdk/GoSystem.h>
#include <kApi/Data/kArray1.h>
#include <kApi/Data/kArray2.h>

namespace gocator
{
namespace
{

std::string messageTypeName(GoPxLSdk::MessageType type)
{
    using GoPxLSdk::MessageType;

    switch (type)
    {
    case MessageType::SIGNAL:
        return "SIGNAL";
    case MessageType::NULL_TYPE:
        return "NULL";
    case MessageType::STAMP:
        return "STAMP";
    case MessageType::UNIFORM_PROFILE:
        return "UNIFORM_PROFILE";
    case MessageType::PROFILE_POINT_CLOUD:
        return "PROFILE_POINT_CLOUD";
    case MessageType::UNIFORM_SURFACE:
        return "UNIFORM_SURFACE";
    case MessageType::SURFACE_POINT_CLOUD:
        return "SURFACE_POINT_CLOUD";
    case MessageType::IMAGE:
        return "IMAGE";
    case MessageType::SPOTS:
        return "SPOTS";
    case MessageType::MESH:
        return "MESH";
    case MessageType::MEASUREMENT:
        return "MEASUREMENT";
    case MessageType::STRING:
        return "STRING";
    case MessageType::RENDERING:
        return "RENDERING";
    case MessageType::POINT_FEATURE:
        return "POINT_FEATURE";
    case MessageType::LINE_FEATURE:
        return "LINE_FEATURE";
    case MessageType::PLANE_FEATURE:
        return "PLANE_FEATURE";
    case MessageType::CIRCLE_FEATURE:
        return "CIRCLE_FEATURE";
    case MessageType::HEALTH:
        return "HEALTH";
    }

    return "UNKNOWN";
}

} // namespace

GocatorAcquisition::GocatorAcquisition(GocatorConnectionConfig config)
    : connection_(std::move(config))
{
}

GocatorAcquisition::~GocatorAcquisition()
{
    disconnect();
}

void GocatorAcquisition::connect()
{
    if (!connection_.isConnected())
    {
        connection_.connect();
    }

    if (!gdpClient_)
    {
        gdpClient_ = std::make_unique<GoPxLSdk::GoGdpClient>();
    }

    if (!gdpClient_->IsConnected())
    {
        gdpClient_->Connect(connection_.system().Address(), connection_.gdpPort());
    }
}

void GocatorAcquisition::disconnect() noexcept
{
    if (gdpClient_)
    {
        try
        {
            if (gdpClient_->IsConnected())
            {
                gdpClient_->Close();
            }
        }
        catch (...)
        {
        }
    }

    connection_.disconnect();
}

bool GocatorAcquisition::isConnected()
{
    return connection_.isConnected() && gdpClient_ && gdpClient_->IsConnected();
}

void GocatorAcquisition::start()
{
    connection_.start();
}

void GocatorAcquisition::stop()
{
    connection_.stop();
}

void GocatorAcquisition::stopNoThrow() noexcept
{
    connection_.stopNoThrow();
}

void GocatorAcquisition::clear()
{
    if (!gdpClient_)
    {
        throw std::logic_error("GDP client is not connected");
    }

    gdpClient_->ClearData();
}

GocatorFrame GocatorAcquisition::receiveOne(int timeoutMs)
{
    if (!gdpClient_ || !gdpClient_->IsConnected())
    {
        throw std::logic_error("GDP client is not connected");
    }

    gdpClient_->ClearData();
    gdpClient_->ReceiveDataSync(static_cast<k64u>(timeoutMs));
    return frameFromDataSet(gdpClient_->DataSet());
}

GocatorFrame GocatorAcquisition::grabOne(int timeoutMs, bool stopAfterReceive)
{
    connect();
    start();

    try
    {
        GocatorFrame frame = receiveOne(timeoutMs);
        if (stopAfterReceive)
        {
            stopNoThrow();
        }
        return frame;
    }
    catch (...)
    {
        if (stopAfterReceive)
        {
            stopNoThrow();
        }
        throw;
    }
}

GocatorFrame GocatorAcquisition::grabUntilValidProfile(int timeoutMs, int maxFrames, bool stopAfterReceive)
{
    connect();
    start();

    try
    {
        GocatorFrame lastFrame;
        const int frameCount = std::max(1, maxFrames);
        for (int i = 0; i < frameCount; ++i)
        {
            lastFrame = receiveOne(timeoutMs);

            if (!lastFrame.images.empty())
            {
                if (stopAfterReceive)
                {
                    stopNoThrow();
                }
                return lastFrame;
            }

            for (const GocatorProfileFrame& profile : lastFrame.profiles)
            {
                if (profile.validCount > 0)
                {
                    if (stopAfterReceive)
                    {
                        stopNoThrow();
                    }
                    return lastFrame;
                }
            }
        }

        if (stopAfterReceive)
        {
            stopNoThrow();
        }
        return lastFrame;
    }
    catch (...)
    {
        if (stopAfterReceive)
        {
            stopNoThrow();
        }
        throw;
    }
}

GocatorFrame GocatorAcquisition::frameFromDataSet(const GoPxLSdk::GoDataSet& dataSet)
{
    GocatorFrame frame;
    frame.messageCount = dataSet.Count();
    frame.messages.reserve(frame.messageCount);

    for (std::size_t index = 0; index < frame.messageCount; ++index)
    {
        const GoPxLSdk::GoGdpMsg& message = dataSet.GdpMsgAt(index);
        frame.messages.push_back(messageInfo(message));

        if (message.Type() == GoPxLSdk::MessageType::IMAGE || message.Type() == GoPxLSdk::MessageType::UNIFORM_SURFACE)
        {
            frame.images.push_back(imageFrame(static_cast<const GoPxLSdk::GoGdpImage&>(message)));
        }
        else if (message.Type() == GoPxLSdk::MessageType::UNIFORM_PROFILE)
        {
            frame.profiles.push_back(uniformProfileFrame(message));
        }
        else if (message.Type() == GoPxLSdk::MessageType::PROFILE_POINT_CLOUD)
        {
            frame.profiles.push_back(pointCloudProfileFrame(message));
        }
        else if (message.Type() == GoPxLSdk::MessageType::SPOTS)
        {
            frame.spots.push_back(spotsFrame(static_cast<const GoPxLSdk::GoGdpSpots&>(message)));
        }
    }

    return frame;
}

GocatorFrameMessage GocatorAcquisition::messageInfo(const GoPxLSdk::GoGdpMsg& message)
{
    GocatorFrameMessage info;
    info.typeName = messageTypeName(message.Type());
    info.typeValue = static_cast<std::uint16_t>(message.Type());
    info.sourceId = message.DataSourceId();
    info.dataSetId = message.DataSetId();
    info.gdpId = message.GdpId();
    info.isLastMessage = message.IsLastMsg();
    return info;
}

GocatorImageFrame GocatorAcquisition::imageFrame(const GoPxLSdk::GoGdpImage& image)
{
    GocatorImageFrame frame;
    frame.sourceId = image.DataSourceId();
    frame.dataSetId = image.DataSetId();
    frame.gdpId = image.GdpId();
    frame.width = image.Width();
    frame.height = image.Height();
    frame.pixelSize = image.PixelSize();
    frame.pixelFormat = static_cast<std::int32_t>(image.PixelFormat());
    frame.pixelFormatName = GoPxLSdk::GoGdpPixelFormat::ToString(image.PixelFormat());

    const kArray2 pixels = image.Pixels();
    if (pixels != kNULL)
    {
        frame.dataSize = kArray2_DataSize(pixels);
        const void* source = kArray2_Data(pixels);
        if (source != nullptr && frame.dataSize > 0)
        {
            frame.pixels.resize(frame.dataSize);
            std::memcpy(frame.pixels.data(), source, frame.dataSize);

            std::uint64_t sum = 0;
            frame.minByte = frame.pixels.front();
            frame.maxByte = frame.pixels.front();
            for (const std::uint8_t value : frame.pixels)
            {
                frame.minByte = std::min(frame.minByte, value);
                frame.maxByte = std::max(frame.maxByte, value);
                sum += value;
            }
            frame.meanByte = static_cast<double>(sum) / static_cast<double>(frame.pixels.size());
            frame.hasByteStats = true;
        }
    }

    return frame;
}

GocatorProfileFrame GocatorAcquisition::uniformProfileFrame(const GoPxLSdk::GoGdpMsg& message)
{
    const auto& profile = static_cast<const GoPxLSdk::GoGdpProfileUniform&>(message);

    GocatorProfileFrame frame;
    frame.sourceId = profile.DataSourceId();
    frame.dataSetId = profile.DataSetId();
    frame.gdpId = profile.GdpId();
    frame.width = profile.Width();
    frame.intensityWidth = profile.IntensityWidth();
    frame.xResolution = profile.Resolution().x;
    frame.zResolution = profile.Resolution().z;
    frame.xOffset = profile.Offset().x;
    frame.zOffset = profile.Offset().z;
    frame.points.resize(frame.width);

    const kArray1 ranges = profile.Ranges();
    const kArray1 intensities = profile.Intensities();
    for (std::uint32_t i = 0; i < frame.width; ++i)
    {
        k16s range = k16S_NULL;
        kArray1_Item(ranges, i, &range);
        if (i == 0)
        {
            frame.firstRange = range;
            frame.minRange = range;
            frame.maxRange = range;
            frame.hasRangeStats = true;
        }
        else
        {
            frame.minRange = std::min(frame.minRange, static_cast<std::int32_t>(range));
            frame.maxRange = std::max(frame.maxRange, static_cast<std::int32_t>(range));
        }

        GocatorProfilePoint& point = frame.points[i];
        point.x = frame.xOffset + frame.xResolution * i;
        if (range != k16S_NULL)
        {
            point.z = frame.zOffset + frame.zResolution * range;
            point.valid = true;
            ++frame.validCount;
        }
        else
        {
            ++frame.nullCount;
        }

        if (intensities != kNULL && i < frame.intensityWidth)
        {
            k16u intensity = 0;
            kArray1_Item(intensities, i, &intensity);
            point.intensity = intensity;
            if (!frame.hasIntensityStats)
            {
                frame.minIntensity = intensity;
                frame.maxIntensity = intensity;
                frame.hasIntensityStats = true;
            }
            else
            {
                frame.minIntensity = std::min(frame.minIntensity, static_cast<std::uint16_t>(intensity));
                frame.maxIntensity = std::max(frame.maxIntensity, static_cast<std::uint16_t>(intensity));
            }
        }
    }

    return frame;
}

GocatorProfileFrame GocatorAcquisition::pointCloudProfileFrame(const GoPxLSdk::GoGdpMsg& message)
{
    const auto& profile = static_cast<const GoPxLSdk::GoGdpProfilePointCloud&>(message);

    GocatorProfileFrame frame;
    frame.sourceId = profile.DataSourceId();
    frame.dataSetId = profile.DataSetId();
    frame.gdpId = profile.GdpId();
    frame.width = profile.Width();
    frame.intensityWidth = profile.IntensityWidth();
    frame.xResolution = profile.Resolution().x;
    frame.zResolution = profile.Resolution().z;
    frame.xOffset = profile.Offset().x;
    frame.zOffset = profile.Offset().z;
    frame.points.resize(frame.width);

    const kArray1 ranges = profile.Ranges();
    const kArray1 intensities = profile.Intensities();
    for (std::uint32_t i = 0; i < frame.width; ++i)
    {
        kPoint16s source = {k16S_NULL, k16S_NULL};
        kArray1_Item(ranges, i, &source);
        if (i == 0)
        {
            frame.firstRange = source.y;
            frame.minRange = source.y;
            frame.maxRange = source.y;
            frame.hasRangeStats = true;
        }
        else
        {
            frame.minRange = std::min(frame.minRange, static_cast<std::int32_t>(source.y));
            frame.maxRange = std::max(frame.maxRange, static_cast<std::int32_t>(source.y));
        }

        GocatorProfilePoint& point = frame.points[i];
        if (source.x != k16S_NULL && source.y != k16S_NULL)
        {
            point.x = frame.xOffset + frame.xResolution * source.x;
            point.z = frame.zOffset + frame.zResolution * source.y;
            point.valid = true;
            ++frame.validCount;
        }
        else
        {
            ++frame.nullCount;
        }

        if (intensities != kNULL && i < frame.intensityWidth)
        {
            k16u intensity = 0;
            kArray1_Item(intensities, i, &intensity);
            point.intensity = intensity;
            if (!frame.hasIntensityStats)
            {
                frame.minIntensity = intensity;
                frame.maxIntensity = intensity;
                frame.hasIntensityStats = true;
            }
            else
            {
                frame.minIntensity = std::min(frame.minIntensity, static_cast<std::uint16_t>(intensity));
                frame.maxIntensity = std::max(frame.maxIntensity, static_cast<std::uint16_t>(intensity));
            }
        }
    }

    return frame;
}

GocatorSpotsFrame GocatorAcquisition::spotsFrame(const GoPxLSdk::GoGdpSpots& spots)
{
    GocatorSpotsFrame frame;
    frame.sourceId = spots.DataSourceId();
    frame.dataSetId = spots.DataSetId();
    frame.gdpId = spots.GdpId();
    frame.pointCount = spots.PointCount();
    frame.exposure = spots.Exposure();
    frame.columnBased = spots.ColumnBased();
    frame.maxSliceCount = spots.MaxSliceCount();
    frame.spotCenterMin = spots.SpotCenterMin();
    frame.spotCenterMax = spots.SpotCenterMax();
    return frame;
}

} // namespace gocator
