#ifndef HYPERION_V2_TERRAIN_EROSION_HPP
#define HYPERION_V2_TERRAIN_EROSION_HPP

#include <terrain/TerrainHeightInfo.hpp>

#include <core/lib/FixedArray.hpp>
#include <core/lib/Pair.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class TerrainErosion
{
    static constexpr UInt num_iterations = 250u;
    static constexpr Float erosion_scale = 0.05f;
    static constexpr Float evaporation = 0.9f;
    static constexpr Float erosion = 0.004f * erosion_scale;
    static constexpr Float deposition = 0.0000002f * erosion_scale;

    static const FixedArray<Pair<Int, Int>, 8> offsets;

public:
    static void Erode(TerrainHeightData &height_data);
};

} // namespace hyperion::v2

#endif