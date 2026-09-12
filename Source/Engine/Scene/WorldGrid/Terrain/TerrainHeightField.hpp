/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Util/NoiseFactory.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class TerrainHeightField
{
public:
    explicit TerrainHeightField(const NoiseCombinator& baseNoise)
        : m_baseNoise(baseNoise)
    {
    }

    HYP_FORCE_INLINE float SampleBaseHeight(const Vec2f& worldXZ) const
    {
        return m_baseNoise.GetNoise(worldXZ);
    }

    void SampleCellHeightsPadded(
        const Vec2f& cellWorldMinXZ,
        const Vec2f& scaleXZ,
        uint32 cellSize,
        Span<const float> sculptDelta,
        Array<float>& outPaddedHeights) const;

private:
    const NoiseCombinator& m_baseNoise;
};

} // namespace Hyperion
