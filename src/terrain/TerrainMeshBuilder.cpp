#include "TerrainMeshBuilder.hpp"
#include <Threads.hpp>
#include <math/MathUtil.hpp>

namespace hyperion::v2 {

TerrainMeshBuilder::TerrainMeshBuilder(const PatchInfo &patch_info)
    : m_patch_info(patch_info)
{
}

void TerrainMeshBuilder::GenerateHeights(const NoiseCombinator &noise_combinator)
{
    Threads::AssertOnThread(THREAD_TERRAIN);

    m_height_infos.Resize(m_patch_info.extent.width * m_patch_info.extent.depth);

    for (int z = 0; z < m_patch_info.extent.depth; z++) {
        for (int x = 0; x < m_patch_info.extent.width; x++) {
            const Float x_offset = static_cast<Float>(x + (m_patch_info.coord.x * (m_patch_info.extent.width - 1))) / static_cast<Float>(m_patch_info.extent.width);
            const Float z_offset = static_cast<Float>(z + (m_patch_info.coord.y * (m_patch_info.extent.depth - 1))) / static_cast<Float>(m_patch_info.extent.depth);

            const UInt index = GetHeightIndex(x, z);

            m_height_infos[index] = {
                .height = noise_combinator.GetNoise(Vector(x_offset, z_offset)),
                .erosion = 0.0f,
                .sediment = 0.0f,
                .water = 1.0f,
                .new_water = 0.0f,
                .down = 0.0f
            };
        }
    }

    // erosion
    // this erosion code based on: https://github.com/RolandR/glterrain

    constexpr bool erosion_enabled = true;

    if constexpr (erosion_enabled) {
        const Float erosion_scale = 1.0f / MathUtil::Max(MathUtil::epsilon<Float>, Vector2(m_patch_info.scale.x, m_patch_info.scale.z).Max());
        constexpr UInt num_erosion_iterations = 250;
        constexpr Float evaporation = 0.9f;
        const Float erosion = 0.004f * erosion_scale;
        const Float deposition = 0.0000002f * erosion_scale;

        static const FixedArray offsets {
            Pair { 1, 0 },
            Pair { 1, 1 },
            Pair { 1, -1 },
            Pair { 0, 1 },
            Pair { 0, -1 },
            Pair { -1, 0 },
            Pair { -1, 1 },
            Pair { -1, -1 },
        };

        for (UInt iteration = 0; iteration < num_erosion_iterations; iteration++) {
            for (int z = 1; z < m_patch_info.extent.depth - 2; z++) {
                for (int x = 1; x < m_patch_info.extent.width - 2; x++) {
                    auto &height_info = m_height_infos[GetHeightIndex(x, z)];

                    Float down = 0.0f;

                    for (const auto &offset : offsets) {
                        down += MathUtil::Max(height_info.height - m_height_infos[GetHeightIndex(x + offset.first, z + offset.second)].height, 0.0f);
                    }

                    height_info.down = down;

                    if (down != 0.0f) {
                        Float water = height_info.water * evaporation;
                        Float staying_water = (water * 0.0002f) / (down * erosion_scale + 1);
                        water = water - staying_water;

                        for (const auto &offset : offsets) {
                            auto &neighbor_height_info = m_height_infos[GetHeightIndex(x + offset.first, z + offset.second)];

                            neighbor_height_info.new_water += MathUtil::Max(height_info.height - neighbor_height_info.height, 0.0f) / down * water;
                        }

                        height_info.water = staying_water + 1.0f;
                    }

                }
            }

            for (int z = 1; z < m_patch_info.extent.depth - 2; z++) {
                for (int x = 1; x < m_patch_info.extent.width - 2; x++) {
                    auto &height_info = m_height_infos[GetHeightIndex(x, z)];

                    height_info.water += height_info.new_water;
                    height_info.new_water = 0.0f;

                    const auto old_height = height_info.height;
                    height_info.height += (-(height_info.down - (0.005f / erosion_scale)) * height_info.water) * erosion + height_info.water * deposition;
                    height_info.erosion = old_height - height_info.height;

                    if (old_height < height_info.height) {
                        height_info.water = MathUtil::Max(height_info.water - (height_info.height - old_height) * 1000.0f, 0.0f);
                    }
                }
            }
        }
    }
}

std::unique_ptr<Mesh> TerrainMeshBuilder::BuildMesh() const
{
    Threads::AssertOnThread(THREAD_TERRAIN);

    std::vector<Vertex> vertices = BuildVertices();
    std::vector<Mesh::Index> indices = BuildIndices();

    auto mesh = std::make_unique<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes // for now
    );

    mesh->CalculateNormals();
    mesh->CalculateTangents();

    return mesh;
}

std::vector<Vertex> TerrainMeshBuilder::BuildVertices() const
{
    std::vector<Vertex> vertices;
    vertices.resize(m_patch_info.extent.width * m_patch_info.extent.depth);

    UInt i = 0;

    for (UInt z = 0; z < m_patch_info.extent.depth; z++) {
        for (UInt x = 0; x < m_patch_info.extent.width; x++) {
            Vector3 position(x, m_height_infos[i].height, z);
            position *= m_patch_info.scale;

            Vector2 texcoord(
               static_cast<float>(x) / static_cast<float>(m_patch_info.extent.width),
               static_cast<float>(z) / static_cast<float>(m_patch_info.extent.depth)
            );

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

std::vector<Mesh::Index> TerrainMeshBuilder::BuildIndices() const
{
    std::vector<Mesh::Index> indices;
    indices.resize(6 * (m_patch_info.extent.width - 1) * (m_patch_info.extent.depth - 1));

    int pitch = m_patch_info.extent.width;
    int row = 0;

    int i0 = row;
    int i1 = row + 1;
    int i2 = pitch + i1;
    int i3 = pitch + row;

    int i = 0;

    for (int z = 0; z < m_patch_info.extent.depth - 1; z++) {
        for (int x = 0; x < m_patch_info.extent.width - 1; x++) {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i1;
            indices[i++] = i2;
            indices[i++] = i0;
            indices[i++] = i3;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

} // namespace hyperion::v2