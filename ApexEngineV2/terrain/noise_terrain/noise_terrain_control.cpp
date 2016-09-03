#include "noise_terrain_control.h"

namespace apex {
NoiseTerrainControl::NoiseTerrainControl(Camera *camera, int seed)
    : TerrainControl(camera), seed(seed)
{
}

TerrainChunk *NoiseTerrainControl::NewChunk(const HeightInfo &height_info)
{
    return new NoiseTerrainChunk(height_info, seed);
}
}