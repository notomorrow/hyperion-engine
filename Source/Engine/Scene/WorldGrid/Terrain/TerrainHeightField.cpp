/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainHeightField.hpp>

namespace Hyperion {

void TerrainHeightField::SampleCellHeightsPadded(
    const Vec2f& cellWorldMinXZ,
    const Vec2f& scaleXZ,
    uint32 cellSize,
    Span<const float> sculptDelta,
    Array<float>& outPaddedHeights) const
{
    const uint32 paddedSize = cellSize + 2;

    outPaddedHeights.Resize(paddedSize * paddedSize);

    const bool hasSculptDelta = sculptDelta.Size() > 0;

    if (hasSculptDelta)
    {
        Assert(sculptDelta.Size() == size_t(cellSize) * size_t(cellSize),
            "Bad sculpt deltas !!! BAD!!!!");

        // for debugging so we don't kill the whole thing
        if (sculptDelta.Size() != size_t(cellSize) * size_t(cellSize))
        {
            return;
        }
    }


    for (uint32 pz = 0; pz < paddedSize; pz++)
    {
        const int32 lz = int32(pz) - 1;

        for (uint32 px = 0; px < paddedSize; px++)
        {
            const int32 lx = int32(px) - 1;

            const Vec2f worldXZ = cellWorldMinXZ + Vec2f(float(lx), float(lz)) * scaleXZ;

            float height = SampleBaseHeight(worldXZ);

            if (hasSculptDelta && lx >= 0 && lx < int32(cellSize) && lz >= 0 && lz < int32(cellSize))
            {
                height += sculptDelta[size_t(lz) * cellSize + size_t(lx)];
            }

            outPaddedHeights[pz * paddedSize + px] = height;
        }
    }
}

} // namespace Hyperion
