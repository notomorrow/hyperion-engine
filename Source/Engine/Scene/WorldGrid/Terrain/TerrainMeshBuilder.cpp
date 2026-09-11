/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>
#include <Scene/WorldGrid/Terrain/TerrainHeightField.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Util/NoiseFactory.hpp>

namespace Hyperion {

#pragma region Helpers

static Array<SimpleVertex> BuildVertices(
    uint32 cellSize,
    const StreamingCellInfo& cellInfo,
    const NoiseCombinator& noise,
    Span<const float> sculptDelta)
{
    TerrainHeightField heightField(noise);

    const Vec2f cellWorldMinXZ(cellInfo.bounds.min.x, cellInfo.bounds.min.z);
    const Vec2f scaleXZ(cellInfo.scale.x, cellInfo.scale.z);

    Array<float> paddedHeights;
    heightField.SampleCellHeightsPadded(cellWorldMinXZ, scaleXZ, cellSize, sculptDelta, paddedHeights);

    const uint32 paddedPitch = cellSize + 2;

    Array<SimpleVertex> vertices;
    vertices.Resize(cellSize * cellSize);

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const uint32 i = z * cellSize + x;

            // padded index for local (x, z) is (x + 1, z + 1)
            const float h = paddedHeights[(z + 1) * paddedPitch + (x + 1)];
            const float hL = paddedHeights[(z + 1) * paddedPitch + x];
            const float hR = paddedHeights[(z + 1) * paddedPitch + (x + 2)];
            const float hD = paddedHeights[z * paddedPitch + (x + 1)];
            const float hU = paddedHeights[(z + 2) * paddedPitch + (x + 1)];

            const Vec3f position = Vec3f { float(x), h, float(z) };
            const Vec2f texcoord(float(x) / float(cellSize), float(z) / float(cellSize));

            // Local (unscaled, per-index-step) tangents -- the renderer's normal matrix accounts
            // for the cell's actual world scale, so normals must be computed in local mesh space.
            const Vec3f tangentX(2.0f, hR - hL, 0.0f);
            const Vec3f tangentZ(0.0f, hU - hD, 2.0f);
            const Vec3f normal = tangentZ.Cross(tangentX).Normalized();

            vertices[i] = SimpleVertex { position, normal, texcoord };
        }
    }

    return vertices;
}

static Array<uint32> BuildIndices(uint32 cellSize)
{
    Array<uint32> indices;
    indices.Resize(size_t(6 * (cellSize - 1) * (cellSize - 1)));

    uint32 pitch = uint32(cellSize);
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < cellSize - 1; z++)
    {
        for (uint32 x = 0; x < cellSize - 1; x++)
        {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i3;
            indices[i++] = i0;
            indices[i++] = i1;
            indices[i++] = i2;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

#pragma endregion Helpers

#pragma region TerrainMeshBuilder

TerrainMeshBuilder::TerrainMeshBuilder(uint32 cellSize)
    : m_cellSize(cellSize)
{
}

TerrainMeshBuilder::~TerrainMeshBuilder() = default;

TerrainMeshBuilder::CellMeshData TerrainMeshBuilder::BuildCellVertexData(
    const StreamingCellInfo& cellInfo,
    const NoiseCombinator& noise,
    Span<const float> sculptDelta) const
{
    CellMeshData result;
    result.vertices = BuildVertices(m_cellSize, cellInfo, noise, sculptDelta);
    result.indices = BuildIndices(m_cellSize);

    return result;
}

#pragma endregion TerrainMeshBuilder

} // namespace Hyperion
