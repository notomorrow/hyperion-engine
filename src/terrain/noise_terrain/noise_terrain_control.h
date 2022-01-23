#ifndef NOISE_TERRAIN_CONTROL_H
#define NOISE_TERRAIN_CONTROL_H

#include "../terrain_control.h"
#include "noise_terrain_chunk.h"

namespace hyperion {

class NoiseTerrainControl : public TerrainControl {
public:
    NoiseTerrainControl(Camera *camera, int seed=123);
    virtual ~NoiseTerrainControl() = default;

protected:
    int seed;

    virtual TerrainChunk *NewChunk(const ChunkInfo &height_info) override;
};

} // namespace hyperion

#endif
