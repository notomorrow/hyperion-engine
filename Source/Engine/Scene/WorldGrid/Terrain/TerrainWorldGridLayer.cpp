/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>
#include <Scene/WorldGrid/Terrain/TerrainHeightField.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetReference.hpp>

#include <Rendering/Material.hpp>

#include <Util/NoiseFactory.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

#include <Framework/EngineGlobals.hpp>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#pragma region TerrainWorldGridLayer

static constexpr float BaseHeight = 6.0f;
static constexpr float MountainHeight = 65.0f;

static constexpr float BaseFrequency = 1.0f / 128.0f;
static constexpr float MountainFrequency = 1.0f / 400.0f;
static constexpr float MountainMaskFrequency = 1.0f / 800.0f;

static UniquePtr<NoiseCombinator> MakeTerrainNoiseCombinator(uint32 seed)
{
    UniquePtr<NoiseCombinator> noiseCombinator = MakeUnique<NoiseCombinator>(seed);

    noiseCombinator->Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, MountainHeight, 0.0f, Vec3f(MountainFrequency, MountainFrequency, 0.0f))
        .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vec3f(MountainMaskFrequency, MountainMaskFrequency, 0.0f))
        .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, BaseHeight, 0.0f, Vec3f(BaseFrequency, BaseFrequency, 0.0f))
        .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.5f, 0.0f, Vec3f(BaseFrequency * 2.0f, BaseFrequency * 2.0f, 0.0f))
        .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.25f, 0.0f, Vec3f(BaseFrequency * 4.0f, BaseFrequency * 4.0f, 0.0f))
        .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.125f, 0.0f, Vec3f(BaseFrequency * 8.0f, BaseFrequency * 8.0f, 0.0f))
        .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, BaseHeight * 0.0625f, 0.0f, Vec3f(BaseFrequency * 16.0f, BaseFrequency * 16.0f, 0.0f));

    return noiseCombinator;
}

static Handle<Scene> MakeTerrainScene()
{
    return MakeHandle<Scene>(NAME("TerrainScene"), SceneFlags::FOREGROUND | SceneFlags::HAS_OCTREE);
}

TerrainWorldGridLayer::TerrainWorldGridLayer()
    : m_scene(MakeTerrainScene())
{
}

TerrainWorldGridLayer::TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo)
    : WorldGridLayer(name, layerInfo),
      m_scene(MakeTerrainScene())
{
}

TerrainWorldGridLayer::~TerrainWorldGridLayer()
{
}

void TerrainWorldGridLayer::SetSeed(uint32 seed)
{
    if (m_noiseCombinator)
    {
        HYP_LOG(WorldGrid, Warning, "Cannot change TerrainWorldGridLayer seed after cells have started streaming");

        return;
    }

    m_layerInfo.seed = seed;
}

void TerrainWorldGridLayer::OnAdded(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());
    
    AssertDebug(m_layerInfo.scale.y == 1.0f, "TerrainWorldGridLayer requires scale.y == 1.0f");

    m_scene->Initialize();

    MaterialAttributes attributes;
    attributes.bucket = RenderBucket::Opaque;
    attributes.flags |= MAF_DEPTH_TEST | MAF_DEPTH_WRITE;

    MaterialParameters parameters;
    parameters.albedo = Vec4f(0.06f, 0.25f, 0.05f, 1.0f);
    parameters.roughness = 0.95f;
    parameters.metalness = 0.0f;

    m_material = MakeHandle<Material>(NAME("TerrainMaterial"), attributes, parameters, MaterialTextures {});
    GetCurrentAssetRegistry()->PutAsset(m_material);

    InitObject(m_material);

    m_noiseCombinator = MakeTerrainNoiseCombinator(m_layerInfo.seed);

    worldGrid->GetWorld()->AddScene(m_scene);
}

void TerrainWorldGridLayer::OnRemoved(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    worldGrid->GetWorld()->RemoveScene(m_scene);
}

Handle<StreamingCell> TerrainWorldGridLayer::CreateStreamingCell(const StreamingCellInfo& cellInfo)
{
    if (!m_scene)
    {
        return Handle<StreamingCell>::Null();
    }

    Handle<TerrainCellData> cellData;

    auto objectsByCoordIt = m_objectsByCoord.Find(cellInfo.coord);

    if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
    {
        cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
    }

    return MakeHandle<TerrainStreamingCell>(cellInfo, m_scene, m_material, HandleFromThis(), cellData);
}

void TerrainWorldGridLayer::RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell)
{
    m_loadedCells[coord] = cell;
}

void TerrainWorldGridLayer::UnregisterLoadedCell(const Vec2i& coord)
{
    m_loadedCells.Erase(coord);
}

static Vec3f ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord)
{
    return Vec3f {
        layerInfo.offset.x + (float(coord.x) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x,
        layerInfo.offset.y,
        layerInfo.offset.z + (float(coord.y) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z
    };
}

void TerrainWorldGridLayer::ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
    const uint32 cellSize = layerInfo.cellSize;
    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return;
    }

    auto coordAt = [&](const Vec2f& worldXZ) -> Vec2f
    {
        return Vec2f(
            (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
            (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f);
    };

    const Vec2f worldPosXZ(worldPos.x, worldPos.z);
    const Vec2f minCoordF = coordAt(worldPosXZ - Vec2f(radius, radius));
    const Vec2f maxCoordF = coordAt(worldPosXZ + Vec2f(radius, radius));

    const int32 minCoordX = int32(MathUtil::Floor(minCoordF.x)) - 1;
    const int32 minCoordZ = int32(MathUtil::Floor(minCoordF.y)) - 1;
    const int32 maxCoordX = int32(MathUtil::Ceil(maxCoordF.x)) + 1;
    const int32 maxCoordZ = int32(MathUtil::Ceil(maxCoordF.y)) + 1;

    for (int32 cz = minCoordZ; cz <= maxCoordZ; cz++)
    {
        for (int32 cx = minCoordX; cx <= maxCoordX; cx++)
        {
            const Vec2i coord(cx, cz);

            const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            const Vec2f closestPoint(
                MathUtil::Clamp(worldPosXZ.x, cellWorldMinXZ.x, cellWorldMaxXZ.x),
                MathUtil::Clamp(worldPosXZ.y, cellWorldMinXZ.y, cellWorldMaxXZ.y));

            if ((closestPoint - worldPosXZ).Length() > radius)
            {
                continue;
            }

            Handle<TerrainCellData> cellData;

            auto objectsByCoordIt = m_objectsByCoord.Find(coord);

            if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
            {
                cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
            }

            const bool isNewCellData = !cellData.IsValid();

            if (isNewCellData)
            {
                cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));
                InitObject(cellData);
            }

            Span<float> delta = cellData->EnsureSculptDelta(cellSize * cellSize);

            if (delta.Size() == 0)
            {
                continue;
            }

            bool anyModified = false;

            int32 minVertexX = int32(cellSize);
            int32 minVertexZ = int32(cellSize);
            int32 maxVertexX = -1;
            int32 maxVertexZ = -1;

            for (uint32 z = 0; z < cellSize; z++)
            {
                for (uint32 x = 0; x < cellSize; x++)
                {
                    const Vec2f vertexWorldXZ = cellWorldMinXZ + Vec2f(float(x), float(z)) * Vec2f(layerInfo.scale.x, layerInfo.scale.z);

                    const float dist = (vertexWorldXZ - worldPosXZ).Length();

                    if (dist > radius)
                    {
                        continue;
                    }

                    const float falloff = 1.0f - (dist / radius);
                    const float weight = falloff * falloff * (3.0f - 2.0f * falloff); // smoothstep

                    delta[z * cellSize + x] += (raise ? 1.0f : -1.0f) * strength * weight;
                    anyModified = true;

                    minVertexX = MathUtil::Min(minVertexX, int32(x));
                    minVertexZ = MathUtil::Min(minVertexZ, int32(z));
                    maxVertexX = MathUtil::Max(maxVertexX, int32(x));
                    maxVertexZ = MathUtil::Max(maxVertexZ, int32(z));
                }
            }

            if (!anyModified)
            {
                continue;
            }

            cellData->MarkDirty();

            if (isNewCellData)
            {
                AddStreamingObject(cellData.Get(), coord);
            }

            m_cellsModifiedSinceStrokeEnd[coord] = true;

            auto loadedCellIt = m_loadedCells.Find(coord);

            if (loadedCellIt != m_loadedCells.End())
            {
                if (Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock(); loadedCell)
                {
                    loadedCell->RebuildMesh(
                        cellData,
                        Vec2i(minVertexX, minVertexZ),
                        Vec2i(maxVertexX, maxVertexZ));
                }
            }
        }
    }
}

float TerrainWorldGridLayer::SampleHeightAt(const Vec2f& worldXZ) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
    const uint32 cellSize = layerInfo.cellSize;

    float height = TerrainHeightField(*m_noiseCombinator).SampleBaseHeight(worldXZ);

    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return height;
    }

    const Vec2f coordF(
        (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
        (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f);

    const Vec2i coord(int32(MathUtil::Floor(coordF.x)), int32(MathUtil::Floor(coordF.y)));

    auto objectsByCoordIt = m_objectsByCoord.Find(coord);

    if (objectsByCoordIt == m_objectsByCoord.End() || !objectsByCoordIt->second.Any())
    {
        return height;
    }

    Handle<TerrainCellData> cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());

    if (!cellData.IsValid())
    {
        return height;
    }

    auto cellDataReadScope = cellData->GetReadScope();

    ConstByteView blob = cellData->GetSculptDelta();

    if (blob.Size() != size_t(cellSize) * size_t(cellSize) * sizeof(float))
    {
        return height;
    }

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    const int32 lx = int32(MathUtil::Floor((worldXZ.x - cellBoundsMin.x) / layerInfo.scale.x + 0.5f));
    const int32 lz = int32(MathUtil::Floor((worldXZ.y - cellBoundsMin.z) / layerInfo.scale.z + 0.5f));

    if (lx < 0 || lx >= int32(cellSize) || lz < 0 || lz >= int32(cellSize))
    {
        return height;
    }

    return height + reinterpret_cast<const float*>(blob.Data())[size_t(lz) * size_t(cellSize) + size_t(lx)];
}

bool TerrainWorldGridLayer::RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    const float slabMinY = layerInfo.offset.y - 256.0f;
    const float slabMaxY = layerInfo.offset.y + MountainHeight * 2.0f + 256.0f;

    float tEnter = 0.0f;
    float tExit = 8192.0f;

    if (MathUtil::Abs(ray.direction.y) > MathUtil::epsilonF)
    {
        const float t0 = (slabMinY - ray.position.y) / ray.direction.y;
        const float t1 = (slabMaxY - ray.position.y) / ray.direction.y;

        tEnter = MathUtil::Max(tEnter, MathUtil::Min(t0, t1));
        tExit = MathUtil::Min(tExit, MathUtil::Max(t0, t1));
    }
    else if (ray.position.y < slabMinY || ray.position.y > slabMaxY)
    {
        return false;
    }

    if (tEnter >= tExit)
    {
        return false;
    }

    const float minStep = MathUtil::Max(MathUtil::Max(layerInfo.scale.x, layerInfo.scale.z) * 0.5f, 0.25f);
    const float maxStep = minStep * 16.0f;

    const auto signedDistance = [&](float t) -> float
    {
        const Vec3f p = ray.position + ray.direction * t;

        return p.y - SampleHeightAt(Vec2f(p.x, p.z));
    };

    float t0 = tEnter;
    float s0 = signedDistance(t0);

    if (s0 <= 0.0f)
    {
        outHitPoint = ray.position + ray.direction * t0;

        return true;
    }

    for (uint32 step = 0; step < 1024 && t0 < tExit; step++)
    {
        const float t1 = MathUtil::Min(t0 + MathUtil::Clamp(s0 * 0.8f, minStep, maxStep), tExit);
        const float s1 = signedDistance(t1);

        if (s1 <= 0.0f)
        {
            float a = t0;
            float b = t1;

            for (uint32 i = 0; i < 8; i++)
            {
                const float mid = (a + b) * 0.5f;

                if (signedDistance(mid) <= 0.0f)
                {
                    b = mid;
                }
                else
                {
                    a = mid;
                }
            }

            outHitPoint = ray.position + ray.direction * b;

            return true;
        }

        t0 = t1;
        s0 = s1;
    }

    return false;
}

void TerrainWorldGridLayer::EndBrushStroke()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (const KeyValuePair<Vec2i, bool>& pair : m_cellsModifiedSinceStrokeEnd)
    {
        auto loadedCellIt = m_loadedCells.Find(pair.first);

        if (loadedCellIt == m_loadedCells.End())
        {
            continue;
        }

        if (Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock(); loadedCell)
        {
            loadedCell->RebuildPickBVH();
        }
    }

    m_cellsModifiedSinceStrokeEnd.Clear();
}

#pragma endregion TerrainWorldGridLayer

} // namespace Hyperion
