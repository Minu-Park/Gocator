#include "GocatorDataSetScene3DAdapter.h"

#include <GoPxLSdk/GoDataSet.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpMsgDef.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpProfilePointCloud.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpProfileUniform.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpStamp.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpSurfacePointCloud.h>
#include <GoPxLSdk/GoGdpMsg/GoGdpSurfaceUniform.h>
#include <kApi/Data/kArray1.h>
#include <kApi/Data/kArray2.h>
#include <kApi/kApiDef.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace
{
#if defined(GRAPHICSENGINE_HAS_OPENMP)
#define GRAPHICSENGINE_OMP_PARALLEL_FOR _Pragma("omp parallel for")
#define GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM _Pragma("omp parallel for reduction(+:validPointCount)")
#else
#define GRAPHICSENGINE_OMP_PARALLEL_FOR
#define GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM
#endif

struct StampInfo
{
    std::uint64_t frameIndex = 0;
    bool valid = false;
};

[[nodiscard]] std::optional<int> positiveInt(const k32u value) noexcept
{
    if (value == 0U || value > static_cast<k32u>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }

    return static_cast<int>(value);
}

[[nodiscard]] std::optional<std::size_t> pixelCount(const int width, const int height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return std::nullopt;
    }

    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

[[nodiscard]] std::string frameId(const GoPxLSdk::GoGdpMsg& msg, const StampInfo& stamp)
{
    if (stamp.valid)
    {
        return std::to_string(stamp.frameIndex);
    }

    return std::to_string(msg.DataSetId());
}

void applyCommonFrameFields(RangeFrame& frame,
                            const int width,
                            const int height,
                            const kPoint3d64f& resolution,
                            const kPoint3d64f& offset,
                            const std::string& id)
{
    frame.width = width;
    frame.height = height;
    frame.xScaleMm = resolution.x != 0.0 ? resolution.x : 1.0;
    frame.yScaleMm = resolution.y != 0.0 ? resolution.y : 1.0;
    frame.zScaleMm = 1.0;
    frame.xOffsetMm = offset.x;
    frame.yOffsetMm = offset.y;
    frame.zOffsetMm = 0.0;
    frame.sensorType = "LMI Gocator";
    frame.frameId = id;
}

[[nodiscard]] std::uint8_t intensityBitsFromItemSize(const kSize itemSize) noexcept
{
    if (itemSize == sizeof(k8u))
    {
        return 8U;
    }
    if (itemSize == sizeof(k16u))
    {
        return 16U;
    }

    return 0U;
}

[[nodiscard]] std::uint8_t copyArray1Intensity(const kArray1 intensities,
                                               const std::size_t width,
                                               std::vector<float>& values)
{
    if (intensities == kNULL || kArray1_Length(intensities) < width)
    {
        return 0U;
    }

    const kSize itemSize = kArray1_ItemSize(intensities);
    const std::uint8_t bits = intensityBitsFromItemSize(itemSize);
    if (bits == 0U)
    {
        return 0U;
    }

    values.resize(width);
    if (itemSize == sizeof(k8u))
    {
        const auto* src = kArray1_DataT(intensities, k8u);
        GRAPHICSENGINE_OMP_PARALLEL_FOR
        for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(width); ++index)
        {
            values[static_cast<std::size_t>(index)] = static_cast<float>(src[index]);
        }
        return bits;
    }

    const auto* src = kArray1_DataT(intensities, k16u);
    GRAPHICSENGINE_OMP_PARALLEL_FOR
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(width); ++index)
    {
        values[static_cast<std::size_t>(index)] = static_cast<float>(src[index]);
    }
    return bits;
}

[[nodiscard]] std::uint8_t copyArray2Intensity(const kArray2 intensities,
                                               const int width,
                                               const int height,
                                               std::vector<float>& values)
{
    const auto count = pixelCount(width, height);
    if (!count.has_value()
        || intensities == kNULL
        || kArray2_Length(intensities, 0) < static_cast<kSize>(height)
        || kArray2_Length(intensities, 1) < static_cast<kSize>(width))
    {
        return 0U;
    }

    const kSize itemSize = kArray2_ItemSize(intensities);
    const std::uint8_t bits = intensityBitsFromItemSize(itemSize);
    if (bits == 0U)
    {
        return 0U;
    }

    values.resize(*count);
    if (itemSize == sizeof(k8u))
    {
        const auto* src = kArray2_DataT(intensities, k8u);
        GRAPHICSENGINE_OMP_PARALLEL_FOR
        for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(*count); ++index)
        {
            values[static_cast<std::size_t>(index)] = static_cast<float>(src[index]);
        }
        return bits;
    }

    const auto* src = kArray2_DataT(intensities, k16u);
    GRAPHICSENGINE_OMP_PARALLEL_FOR
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(*count); ++index)
    {
        values[static_cast<std::size_t>(index)] = static_cast<float>(src[index]);
    }
    return bits;
}

[[nodiscard]] StampInfo readStamp(const GoPxLSdk::GoDataSet& dataSet)
{
    for (std::size_t index = 0; index < dataSet.Count(); ++index)
    {
        const GoPxLSdk::GoGdpMsg& msg = dataSet.GdpMsgAt(index);
        if (msg.Type() == GoPxLSdk::MessageType::STAMP)
        {
            const auto& stamp = static_cast<const GoPxLSdk::GoGdpStamp&>(msg);
            return {static_cast<std::uint64_t>(stamp.FrameIndex()), true};
        }
    }

    return {};
}

[[nodiscard]] GraphicsScene3D makeScene(RangeFrame&& frame,
                                        const GoPxLSdk::GoGdpMsg& msg,
                                        const StampInfo& stamp,
                                        const GraphicsScene3DRequest& request)
{
    GraphicsScene3D scene;
    scene.content = GraphicsScene3DContent::RangeFrame;
    scene.rangeFrame = std::move(frame);
    scene.meta.sourceName = "LMI Gocator";
    scene.meta.frameId = frameId(msg, stamp);
    scene.meta.frameIndex = stamp.valid ? stamp.frameIndex : msg.DataSetId();
    scene.meta.retainSurfaceMesh = request.retainSurfaceMesh;
    return scene;
}

[[nodiscard]] std::optional<RangeFrame> convertUniformSurface(
    const GoPxLSdk::GoGdpSurfaceUniform& msg,
    const StampInfo& stamp,
    const GraphicsScene3DRequest& request)
{
    const auto width = positiveInt(msg.Width());
    const auto height = positiveInt(msg.Length());
    if (!width.has_value() || !height.has_value())
    {
        return std::nullopt;
    }

    const auto count = pixelCount(*width, *height);
    const kArray2 ranges = msg.Ranges();
    if (!count.has_value()
        || ranges == kNULL
        || kArray2_ItemSize(ranges) != sizeof(k16s)
        || kArray2_Length(ranges, 0) < static_cast<kSize>(*height)
        || kArray2_Length(ranges, 1) < static_cast<kSize>(*width))
    {
        return std::nullopt;
    }

    RangeFrame frame;
    applyCommonFrameFields(frame, *width, *height, msg.Resolution(), msg.Offset(), frameId(msg, stamp));
    frame.zValues.resize(*count);
    frame.validMask.resize(*count, 1U);

    const kPoint3d64f resolution = msg.Resolution();
    const kPoint3d64f offset = msg.Offset();
    const auto* src = kArray2_DataT(ranges, k16s);
    std::size_t validPointCount = 0U;

    GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(*count); ++index)
    {
        const auto dstIndex = static_cast<std::size_t>(index);
        const k16s raw = src[dstIndex];
        if (raw == k16S_NULL)
        {
            frame.zValues[dstIndex] = 0.0F;
            frame.validMask[dstIndex] = 0U;
            continue;
        }

        frame.zValues[dstIndex] = static_cast<float>(offset.z + resolution.z * static_cast<double>(raw));
        ++validPointCount;
    }

    if (validPointCount == *count)
    {
        frame.validMask.clear();
    }

    if (request.includeRangeAuxiliaryChannels)
    {
        frame.intensityBits = copyArray2Intensity(msg.Intensities(), *width, *height, frame.intensity);
    }

    return frame.isValid() ? std::optional<RangeFrame>(std::move(frame)) : std::nullopt;
}

[[nodiscard]] std::optional<RangeFrame> convertSurfacePointCloud(
    const GoPxLSdk::GoGdpSurfacePointCloud& msg,
    const StampInfo& stamp,
    const GraphicsScene3DRequest& request)
{
    const auto width = positiveInt(msg.Width());
    const auto height = positiveInt(msg.Length());
    if (!width.has_value() || !height.has_value())
    {
        return std::nullopt;
    }

    const auto count = pixelCount(*width, *height);
    const kArray2 ranges = msg.Ranges();
    if (!count.has_value()
        || ranges == kNULL
        || kArray2_ItemSize(ranges) != sizeof(kPoint3d16s)
        || kArray2_Length(ranges, 0) < static_cast<kSize>(*height)
        || kArray2_Length(ranges, 1) < static_cast<kSize>(*width))
    {
        return std::nullopt;
    }

    RangeFrame frame;
    applyCommonFrameFields(frame, *width, *height, msg.Resolution(), msg.Offset(), frameId(msg, stamp));
    frame.xValues.resize(*count);
    frame.yValues.resize(*count);
    frame.zValues.resize(*count);
    frame.validMask.resize(*count, 1U);

    const kPoint3d64f resolution = msg.Resolution();
    const kPoint3d64f offset = msg.Offset();
    const auto* src = kArray2_DataT(ranges, kPoint3d16s);
    std::size_t validPointCount = 0U;

    GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(*count); ++index)
    {
        const auto dstIndex = static_cast<std::size_t>(index);
        const kPoint3d16s point = src[dstIndex];
        if (point.x == k16S_NULL || point.y == k16S_NULL || point.z == k16S_NULL)
        {
            frame.xValues[dstIndex] = 0.0F;
            frame.yValues[dstIndex] = 0.0F;
            frame.zValues[dstIndex] = 0.0F;
            frame.validMask[dstIndex] = 0U;
            continue;
        }

        frame.xValues[dstIndex] = static_cast<float>(offset.x + resolution.x * static_cast<double>(point.x));
        frame.yValues[dstIndex] = static_cast<float>(offset.y + resolution.y * static_cast<double>(point.y));
        frame.zValues[dstIndex] = static_cast<float>(offset.z + resolution.z * static_cast<double>(point.z));
        ++validPointCount;
    }

    if (validPointCount == *count)
    {
        frame.validMask.clear();
    }

    if (request.includeRangeAuxiliaryChannels)
    {
        frame.intensityBits = copyArray2Intensity(msg.Intensities(), *width, *height, frame.intensity);
    }

    return frame.isValid() ? std::optional<RangeFrame>(std::move(frame)) : std::nullopt;
}

[[nodiscard]] std::optional<RangeFrame> convertUniformProfile(
    const GoPxLSdk::GoGdpProfileUniform& msg,
    const StampInfo& stamp,
    const GraphicsScene3DRequest& request)
{
    const auto width = positiveInt(msg.Width());
    if (!width.has_value())
    {
        return std::nullopt;
    }

    const std::size_t count = static_cast<std::size_t>(*width);
    const kArray1 ranges = msg.Ranges();
    if (ranges == kNULL || kArray1_ItemSize(ranges) != sizeof(k16s) || kArray1_Length(ranges) < count)
    {
        return std::nullopt;
    }

    RangeFrame frame;
    applyCommonFrameFields(frame, *width, 1, msg.Resolution(), msg.Offset(), frameId(msg, stamp));
    frame.yScaleMm = 1.0;
    frame.zValues.resize(count);
    frame.validMask.resize(count, 1U);

    const kPoint3d64f resolution = msg.Resolution();
    const kPoint3d64f offset = msg.Offset();
    const auto* src = kArray1_DataT(ranges, k16s);
    std::size_t validPointCount = 0U;

    GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(count); ++index)
    {
        const auto dstIndex = static_cast<std::size_t>(index);
        const k16s raw = src[dstIndex];
        if (raw == k16S_NULL)
        {
            frame.zValues[dstIndex] = 0.0F;
            frame.validMask[dstIndex] = 0U;
            continue;
        }

        frame.zValues[dstIndex] = static_cast<float>(offset.z + resolution.z * static_cast<double>(raw));
        ++validPointCount;
    }

    if (validPointCount == count)
    {
        frame.validMask.clear();
    }

    if (request.includeRangeAuxiliaryChannels)
    {
        frame.intensityBits = copyArray1Intensity(msg.Intensities(), count, frame.intensity);
    }

    return frame.isValid() ? std::optional<RangeFrame>(std::move(frame)) : std::nullopt;
}

[[nodiscard]] std::optional<RangeFrame> convertProfilePointCloud(
    const GoPxLSdk::GoGdpProfilePointCloud& msg,
    const StampInfo& stamp,
    const GraphicsScene3DRequest& request)
{
    const auto width = positiveInt(msg.Width());
    if (!width.has_value())
    {
        return std::nullopt;
    }

    const std::size_t count = static_cast<std::size_t>(*width);
    const kArray1 ranges = msg.Ranges();
    if (ranges == kNULL || kArray1_ItemSize(ranges) != sizeof(kPoint16s) || kArray1_Length(ranges) < count)
    {
        return std::nullopt;
    }

    RangeFrame frame;
    applyCommonFrameFields(frame, *width, 1, msg.Resolution(), msg.Offset(), frameId(msg, stamp));
    frame.yScaleMm = 1.0;
    frame.xValues.resize(count);
    frame.yValues.resize(count, 0.0F);
    frame.zValues.resize(count);
    frame.validMask.resize(count, 1U);

    const kPoint3d64f resolution = msg.Resolution();
    const kPoint3d64f offset = msg.Offset();
    const auto* src = kArray1_DataT(ranges, kPoint16s);
    std::size_t validPointCount = 0U;

    GRAPHICSENGINE_OMP_PARALLEL_FOR_REDUCTION_SUM
    for (std::ptrdiff_t index = 0; index < static_cast<std::ptrdiff_t>(count); ++index)
    {
        const auto dstIndex = static_cast<std::size_t>(index);
        const kPoint16s point = src[dstIndex];
        if (point.x == k16S_NULL || point.y == k16S_NULL)
        {
            frame.xValues[dstIndex] = 0.0F;
            frame.zValues[dstIndex] = 0.0F;
            frame.validMask[dstIndex] = 0U;
            continue;
        }

        frame.xValues[dstIndex] = static_cast<float>(offset.x + resolution.x * static_cast<double>(point.x));
        frame.zValues[dstIndex] = static_cast<float>(offset.z + resolution.z * static_cast<double>(point.y));
        ++validPointCount;
    }

    if (validPointCount == count)
    {
        frame.validMask.clear();
    }

    if (request.includeRangeAuxiliaryChannels)
    {
        frame.intensityBits = copyArray1Intensity(msg.Intensities(), count, frame.intensity);
    }

    return frame.isValid() ? std::optional<RangeFrame>(std::move(frame)) : std::nullopt;
}
}

std::optional<GraphicsScene3D> GocatorDataSetScene3DAdapter::convertScene3D(
    const GoPxLSdk::GoDataSet& dataSet,
    const GraphicsScene3DRequest& request) const
{
    if (!hasScene3DContent(request.content, GraphicsScene3DContent::RangeFrame))
    {
        return std::nullopt;
    }

    const StampInfo stamp = readStamp(dataSet);
    for (std::size_t index = 0; index < dataSet.Count(); ++index)
    {
        const GoPxLSdk::GoGdpMsg& msg = dataSet.GdpMsgAt(index);
        std::optional<RangeFrame> frame;

        switch (msg.Type())
        {
        case GoPxLSdk::MessageType::UNIFORM_SURFACE:
            frame = convertUniformSurface(static_cast<const GoPxLSdk::GoGdpSurfaceUniform&>(msg), stamp, request);
            break;

        case GoPxLSdk::MessageType::SURFACE_POINT_CLOUD:
            frame = convertSurfacePointCloud(static_cast<const GoPxLSdk::GoGdpSurfacePointCloud&>(msg), stamp, request);
            break;

        case GoPxLSdk::MessageType::UNIFORM_PROFILE:
            frame = convertUniformProfile(static_cast<const GoPxLSdk::GoGdpProfileUniform&>(msg), stamp, request);
            break;

        case GoPxLSdk::MessageType::PROFILE_POINT_CLOUD:
            frame = convertProfilePointCloud(static_cast<const GoPxLSdk::GoGdpProfilePointCloud&>(msg), stamp, request);
            break;

        default:
            break;
        }

        if (frame.has_value())
        {
            return makeScene(std::move(*frame), msg, stamp, request);
        }
    }

    return std::nullopt;
}
