#ifndef NOISE_TERRAIN_CHUNK_H
#define NOISE_TERRAIN_CHUNK_H

#include "../terrain_chunk.h"

#include <vector>

namespace apex {
class NoiseTerrainChunk : public TerrainChunk {
public:
    NoiseTerrainChunk(const ChunkInfo &chunk_info, int seed);

    virtual int HeightIndexAt(int x, int z) override;

private:
    std::vector<double> heights;
};
} // namespace apex

#endif