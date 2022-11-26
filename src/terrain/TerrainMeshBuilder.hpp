#ifndef HYPERION_V2_TERRAIN_MESH_BUILDER_H
#define HYPERION_V2_TERRAIN_MESH_BUILDER_H

#include <core/Containers.hpp>
#include <rendering/Mesh.hpp>
#include <scene/controllers/PagingController.hpp>
#include <util/NoiseFactory.hpp>

#include <terrain/TerrainHeightInfo.hpp>

#include <memory>
#include <vector>

namespace hyperion::v2 {

class PatchInfo;

class TerrainMeshBuilder
{
public:
    TerrainMeshBuilder(const PatchInfo &patch_info);
    TerrainMeshBuilder(const TerrainMeshBuilder &other) = delete;
    TerrainMeshBuilder &operator=(const TerrainMeshBuilder &other) = delete;
    ~TerrainMeshBuilder() = default;

    void GenerateHeights(const NoiseCombinator &noise_combinator);
    Handle<Mesh> BuildMesh() const;

private:
    std::vector<Vertex> BuildVertices() const;
    std::vector<Mesh::Index> BuildIndices() const;

    TerrainHeightData m_height_data;
};

} // namespace hyperion::v2

#endif