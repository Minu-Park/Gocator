#pragma once

/**
 * @file GocatorDataSetScene3DAdapter.h
 * @brief LMI Gocator GDP dataset adapter for neutral GraphicsEngine scene data.
 */

#include "engine/adapters/Scene3DAdapter.h"
#include "engine/GraphicsSceneTypes.h"

#include <optional>

namespace GoPxLSdk
{
class GoDataSet;
}

class GocatorDataSetScene3DAdapter final
    : public Scene3DAdapter<GocatorDataSetScene3DAdapter, GoPxLSdk::GoDataSet>
{
public:
    GocatorDataSetScene3DAdapter() = default;
    ~GocatorDataSetScene3DAdapter() = default;

    using Scene3DAdapter<GocatorDataSetScene3DAdapter, GoPxLSdk::GoDataSet>::convert;

private:
    friend class Scene3DAdapter<GocatorDataSetScene3DAdapter, GoPxLSdk::GoDataSet>;

    [[nodiscard]] std::optional<GraphicsScene3D> convertScene3D(
        const GoPxLSdk::GoDataSet& dataSet,
        const GraphicsScene3DRequest& request) const;
};
