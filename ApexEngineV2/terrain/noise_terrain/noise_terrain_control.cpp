#include "noise_terrain_control.h"

namespace apex {

NoiseTerrainControl::NoiseTerrainControl(Camera *camera, int seed)
    : TerrainControl(camera), 
      seed(seed)
{
}

TerrainChunk *NoiseTerrainControl::NewChunk(const ChunkInfo &chunk_info)
{
    const std::vector<double> heights = NoiseTerrainChunk::GenerateHeights(seed, chunk_info);
    return new NoiseTerrainChunk(heights, chunk_info);
}

} // namespace apex