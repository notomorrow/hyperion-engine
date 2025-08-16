/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <streaming/Streamable.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class WorldGrid;
class Scene;

HYP_ENUM()
enum class StreamingCellState : uint32
{
    INVALID = ~0u,

    UNLOADED = 0,
    UNLOADING,
    WAITING,
    LOADING,
    LOADED,

    MAX
};

HYP_STRUCT()
struct StreamingCellNeighbor
{
    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FORCE_INLINE Vec2f GetCenter() const
    {
        return Vec2f(coord) - 0.5f;
    }
};

HYP_STRUCT(Size = 80)
struct StreamingCellInfo
{
    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FIELD(Serialize, Property = "Extent")
    Vec3u extent;

    HYP_FIELD(Serialize, Property = "Scale")
    Vec3f scale = Vec3f::One();

    HYP_FIELD(Serialize, Property = "Bounds")
    BoundingBox bounds;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(coord);
        hashCode.Add(extent);
        hashCode.Add(scale);
        hashCode.Add(bounds);

        return hashCode;
    }
};

HYP_CLASS()
class HYP_API StreamingCell : public StreamableBase
{
    HYP_OBJECT_BODY(StreamingCell);

public:
    StreamingCell() = default;
    StreamingCell(const StreamingCellInfo& cellInfo);
    virtual ~StreamingCell() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const StreamingCellInfo& GetPatchInfo() const
    {
        return m_cellInfo;
    }

    HYP_METHOD(Scriptable)
    void Update(float delta);

protected:
    HYP_METHOD()
    virtual BoundingBox GetBoundingBox_Impl() const override
    {
        return m_cellInfo.bounds;
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
    }

    StreamingCellInfo m_cellInfo;
};

} // namespace hyperion

