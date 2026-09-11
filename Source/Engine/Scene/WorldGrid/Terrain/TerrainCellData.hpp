/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Utilities/Span.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "Terrain")
class ENGINE_API TerrainCellData : public AssetObject
{
    HYP_OBJECT_BODY(TerrainCellData);

public:
    TerrainCellData();
    explicit TerrainCellData(Name name, const Vec2i& coord = Vec2i::Zero(), const Vec3u& extent = Vec3u::Zero());

    TerrainCellData(const TerrainCellData& other) = delete;
    TerrainCellData& operator=(const TerrainCellData& other) = delete;

    TerrainCellData(TerrainCellData&& other) noexcept = delete;
    TerrainCellData& operator=(TerrainCellData&& other) noexcept = delete;

    ~TerrainCellData();

    HYP_FIELD(Property = "Coord", Serialize)
    Vec2i coord;

    HYP_FIELD(Property = "Extent", Serialize)
    Vec3u extent;

    void SetSculptDelta(ConstByteView view);

    ConstByteView GetSculptDelta() const;
    Span<const float> GetSculptDeltaFloats() const;

    /*! Ensures a writable sculpt delta buffer exists with space for \p numVertices vertices,
     *  paging persisted data in from disk when required. Allocation happens under a write
     *  scope, so the caller must not hold any scope on this asset when calling (a held read
     *  scope would deadlock the writer lock).
     *  Returns true when the buffer is ready; use GetSculptDeltaMutable() under a write scope
     *  to mutate it. */
    bool EnsureWritableSculptDelta(uint32 numVertices);

    /*! Mutable view over the sculpt delta. Only valid while a scope that keeps the blob data
     *  resident is held. Returns an empty span when no delta is resident. */
    Span<float> GetSculptDeltaMutable();

protected:
    virtual void Init() override;

    virtual void PageBlobData() override;
    virtual void UnpageBlobData() override;

    virtual void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        // terrain sculpt deltas
        outReferences.EmplaceBack("TERA", 1, &m_sculptDelta);
    }

private:
    HYP_FIELD(Property = "SculptDelta", Serialize)
    BlobDataReference m_sculptDelta;
};

} // namespace Hyperion
