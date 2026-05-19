#include "gocator/GocatorAcquisition.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#include <GoPxLSdk/GoDataSet.h>
#include <GoPxLSdk/GoGdpClient.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpImage.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpPixelFormat.h>
#include <GoPxLSdk/GoSystem.h>
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

GocatorFrame GocatorAcquisition::frameFromDataSet(const GoPxLSdk::GoDataSet& dataSet)
{
    GocatorFrame frame;
    frame.messageCount = dataSet.Count();
    frame.messages.reserve(frame.messageCount);

    for (std::size_t index = 0; index < frame.messageCount; ++index)
    {
        const GoPxLSdk::GoGdpMsg& message = dataSet.GdpMsgAt(index);
        frame.messages.push_back(messageInfo(message));

        if (message.Type() == GoPxLSdk::MessageType::IMAGE)
        {
            frame.images.push_back(imageFrame(static_cast<const GoPxLSdk::GoGdpImage&>(message)));
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
        }
    }

    return frame;
}

} // namespace gocator
