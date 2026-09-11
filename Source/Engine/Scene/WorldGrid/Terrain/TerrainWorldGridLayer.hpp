/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Containers/FlatMap.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class Material;
class Mesh;
class Scene;
class NoiseCombinator;
class TerrainStreamingCell;
class TerrainCellData;

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

public:
    TerrainWorldGridLayer();

    explicit TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo = {});

    virtual ~TerrainWorldGridLayer() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    HYP_FORCE_INLINE const NoiseCombinator& GetNoiseCombinator() const
    {
        return *m_noiseCombinator;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE uint32 GetSeed() const
    {
        return m_layerInfo.seed;
    }

    HYP_METHOD()
    void SetSeed(uint32 seed);

    HYP_METHOD()
    void ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise);

    /*! Samples the full terrain height (base noise + sculpt delta) at the given world XZ position. */
    float SampleHeightAt(const Vec2f& worldXZ) const;

    /*! Marches the ray against the CPU-side height field (independent of mesh BVHs), so hit points
     *  always reflect sculpted geometry, even while a stroke is in progress. */
    bool RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const;

    /*! Called when a sculpt stroke ends; rebuilds picking BVHs for all cells modified since the
     *  previous call. */
    void EndBrushStroke();

    void RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell);
    void UnregisterLoadedCell(const Vec2i& coord);

protected:
    virtual void OnAdded(WorldGrid* worldGrid) override;
    virtual void OnRemoved(WorldGrid* worldGrid) override;

    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo) override;

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    UniquePtr<NoiseCombinator> m_noiseCombinator;
    FlatMap<Vec2i, WeakHandle<TerrainStreamingCell>> m_loadedCells;
    FlatMap<Vec2i, bool> m_cellsModifiedSinceStrokeEnd;

    struct DeltaSampleCache
    {
        Handle<TerrainCellData> cell;
        TSharedResLock<AssetObject> scope;
        ConstByteView blobData;

        void Invalidate();
    };
    
    mutable DeltaSampleCache m_deltaSampleCache;
};

} // namespace Hyperion
