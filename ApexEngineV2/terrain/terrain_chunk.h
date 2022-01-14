#ifndef TERRAIN_CHUNK_H
#define TERRAIN_CHUNK_H

#include "height_info.h"
#include "../rendering/mesh.h"
#include "../rendering/vertex.h"
#include "../entity.h"

#include <memory>

namespace apex {

class TerrainChunk {
public:
    TerrainChunk(const ChunkInfo &chunk_info);
    virtual ~TerrainChunk() = default;

    virtual void OnAdded() = 0;
    // to be implemented by derived class, as the way of generating heights
    virtual int HeightIndexAt(int x, int z) = 0;

    ChunkInfo m_chunk_info;
    std::shared_ptr<Entity> m_entity = nullptr;

protected:

    std::shared_ptr<Mesh> BuildMesh(const std::vector<double> &heights);
    void AddNormal(Vertex &vertex, const Vector3 &normal);
    void CalculateNormals(std::vector<Vertex> &vertices, const std::vector<MeshIndex> &indices);
    std::vector<Vertex> BuildVertices(const std::vector<double> &heights);
    std::vector<MeshIndex> BuildIndices();
};

} // namespace apex

#endif
