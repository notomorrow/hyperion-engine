#ifndef NOISE_TERRAIN_CHUNK_H
#define NOISE_TERRAIN_CHUNK_H

#define OSN_OCTAVE_COUNT 8

#include "../terrain_chunk.h"

#include <vector>

struct osn_context;

namespace apex {

struct SimplexNoiseData {
    osn_context *octaves[OSN_OCTAVE_COUNT];
    double frequencies[OSN_OCTAVE_COUNT];
    double amplitudes[OSN_OCTAVE_COUNT];
};

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

    static double GetSimplexNoise(SimplexNoiseData *data, int x, int z);
};

} // namespace apex

#endif
