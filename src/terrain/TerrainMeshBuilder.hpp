#ifndef HYPERION_V2_TERRAIN_MESH_BUILDER_H
#define HYPERION_V2_TERRAIN_MESH_BUILDER_H

#include <core/Containers.hpp>
#include <rendering/Mesh.hpp>
#include <scene/controllers/PagingController.hpp>
#include <util/NoiseFactory.hpp>

#include <memory>
#include <vector>

namespace hyperion::v2 {

class PatchInfo;

class TerrainMeshBuilder {
public:
    TerrainMeshBuilder(const PatchInfo &patch_info);
    TerrainMeshBuilder(const TerrainMeshBuilder &other) = delete;
    TerrainMeshBuilder &operator=(const TerrainMeshBuilder &other) = delete;
    ~TerrainMeshBuilder() = default;

    void GenerateHeights(const NoiseCombinator &noise_combinator);
    std::unique_ptr<Mesh> BuildMesh() const;

private:
    struct TerrainHeightInfo {
        Float height;
        Float erosion;
        Float sediment;
        Float water;
        Float new_water;
        Float down;
    };

    UInt GetHeightIndex(UInt x, UInt z) const
    {
        return ((x + m_patch_info.extent.width) % m_patch_info.extent.width)
                + ((z + m_patch_info.extent.depth) % m_patch_info.extent.depth) * m_patch_info.extent.width;
    }

    std::vector<Vertex> BuildVertices() const;
    std::vector<Mesh::Index> BuildIndices() const;

    PatchInfo m_patch_info;
    DynArray<TerrainHeightInfo> m_height_infos;
};

} // namespace hyperion::v2

#endif