#ifndef NOISE_TERRAIN_CONTROL_H
#define NOISE_TERRAIN_CONTROL_H

#include "../terrain_control.h"
#include "noise_terrain_chunk.h"

namespace apex {
class NoiseTerrainControl : public TerrainControl {
public:
    NoiseTerrainControl(Camera *camera, int seed=123);

protected:
    int seed;

    TerrainChunk *NewChunk(const HeightInfo &height_info);
};
}

#endif