#include "noise_terrain_control.h"

namespace apex {

NoiseTerrainControl::NoiseTerrainControl(Camera *camera, int seed)
    : TerrainControl(camera), 
      seed(seed)
{
}

TerrainChunk *NoiseTerrainControl::NewChunk(const ChunkInfo &chunk_info)
{
    return new NoiseTerrainChunk(chunk_info, seed);
}

} // namespace apex