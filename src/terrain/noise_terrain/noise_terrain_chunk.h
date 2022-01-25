#ifndef NOISE_TERRAIN_CHUNK_H
#define NOISE_TERRAIN_CHUNK_H

#include "../../util/random/simplex.h"

#include "../terrain_chunk.h"

#include <vector>


namespace hyperion {

class NoiseTerrainChunk : public TerrainChunk {
public:
    static std::vector<double> GenerateHeights(int seed, const ChunkInfo &chunk_info);

public:
    NoiseTerrainChunk(const std::vector<double> &heights, const ChunkInfo &chunk_info);
    virtual ~NoiseTerrainChunk() = default;

    virtual void OnAdded() override;

private:
    std::vector<double> m_heights;

    virtual Vector4 BiomeAt(int x, int z) override;

    static SimplexNoiseData CreateSimplexNoise(int seed);
    static void FreeSimplexNoise(SimplexNoiseData *data);
    static double GetSimplexNoise(SimplexNoiseData *data, int x, int z);
};

} // namespace hyperion

#endif
