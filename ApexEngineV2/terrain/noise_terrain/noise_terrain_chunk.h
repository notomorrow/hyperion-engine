#ifndef NOISE_TERRAIN_CHUNK_H
#define NOISE_TERRAIN_CHUNK_H

#include "../terrain_chunk.h"

#include <vector>

namespace apex {

class NoiseTerrainChunk : public TerrainChunk {
public:
    static std::vector<double> GenerateHeights(int seed, const ChunkInfo &chunk_info);

public:
    NoiseTerrainChunk(const std::vector<double> &heights, const ChunkInfo &chunk_info);
    virtual ~NoiseTerrainChunk() = default;

    virtual void OnAdded() override;
    virtual int HeightIndexAt(int x, int z) override;

private:
    std::vector<double> m_heights;
};

} // namespace apex

#endif