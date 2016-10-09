#include "noise_terrain_chunk.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../rendering/environment.h"
#include "../../util/random/worley_noise_generator.h"

#include <noise/noise.h>
#include <noise/module/ridgedmulti.h>
using namespace noise;

#include "../../util/random/open_simplex_noise.h"

#define MOUNTAIN_SCALE_WIDTH 0.04
#define MOUNTAIN_SCALE_LENGTH 0.04
#define MOUNTAIN_SCALE_HEIGHT 8.0

#define ROUGH_SCALE_WIDTH 0.8
#define ROUGH_SCALE_LENGTH 0.8
#define ROUGH_SCALE_HEIGHT 1.3

#define SMOOTH_SCALE_WIDTH 0.08
#define SMOOTH_SCALE_LENGTH 0.08
#define SMOOTH_SCALE_HEIGHT 1.0

#define MASK_SCALE_WIDTH 0.02
#define MASK_SCALE_LENGTH 0.02

namespace apex {
NoiseTerrainChunk::NoiseTerrainChunk(const ChunkInfo &chunk_info, int seed)
    : TerrainChunk(chunk_info)
{
    module::RidgedMulti multi;
    multi.SetSeed(seed);
    multi.SetFrequency(0.03);
    multi.SetNoiseQuality(NoiseQuality::QUALITY_FAST);
    multi.SetOctaveCount(11);
    multi.SetLacunarity(2.0);

    WorleyNoiseGenerator worley(seed);

    module::Perlin maskgen;
    maskgen.SetFrequency(0.05);
    maskgen.SetPersistence(0.25);

    struct osn_context *ctx;
    open_simplex_noise(seed, &ctx);

    heights.resize((chunk_info.m_width) * (chunk_info.m_length));

    for (int z = 0; z < chunk_info.m_length; z++) {
        for (int x = 0; x < chunk_info.m_width; x++) {
            int x_offset = x + ((int)chunk_info.m_position.x * (chunk_info.m_width - 1));
            int z_offset = z + ((int)chunk_info.m_position.y * (chunk_info.m_length - 1));

            //double smooth = (open_simplex_noise2(ctx, x_offset * SMOOTH_SCALE_WIDTH, 
            //    z_offset * SMOOTH_SCALE_HEIGHT) * 2.0 - 1.0) * SMOOTH_SCALE_HEIGHT;

            //double mask = maskgen.GetValue(x_offset * MASK_SCALE_WIDTH,
            //    z_offset * MASK_SCALE_LENGTH, 0.0);

            double rough = (multi.GetValue(x_offset * ROUGH_SCALE_WIDTH,
                z_offset * ROUGH_SCALE_LENGTH, 0.0) * 2.0 - 1.0) * ROUGH_SCALE_HEIGHT;

            double mountain = (worley.Noise(x_offset * MOUNTAIN_SCALE_WIDTH,
                z_offset * MOUNTAIN_SCALE_LENGTH, 0.0) * 2.0 - 1.0) * MOUNTAIN_SCALE_HEIGHT;

            heights[HeightIndexAt(x, z)] = rough + mountain;
        }
    }

    open_simplex_noise_free(ctx);

    auto mesh = BuildMesh(heights);
    mesh->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
        { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
        { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
    }));
    m_entity = std::make_shared<Entity>("terrain_node");
    m_entity->SetRenderable(mesh);
}

int NoiseTerrainChunk::HeightIndexAt(int x, int z)
{
    int size = m_chunk_info.m_width;
    return ((x + size) % size) + ((z + size) % size) * size;
}
} // namespace apex