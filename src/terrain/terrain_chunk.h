#ifndef TERRAIN_CHUNK_H
#define TERRAIN_CHUNK_H

#include "height_info.h"
#include "../rendering/mesh.h"
#include "../math/vertex.h"
#include "../entity.h"

#include <memory>

namespace hyperion {

class TerrainChunk : public Entity {
public:
    TerrainChunk(const ChunkInfo &chunk_info);
    virtual ~TerrainChunk() = default;

    virtual void OnAdded() = 0;

    int HeightIndexAt(int x, int z) const;
    int HeightIndexAtWorld(const Vector3 &world) const;
    double HeightAtIndex(int index) const;
    double HeightAt(int x, int z) const;
    double HeightAtWorld(const Vector3 &world) const;

    Vector3 NormalAtIndex(int index) const;
    Vector3 NormalAt(int x, int z) const;
    Vector3 NormalAtWorld(const Vector3 &world) const;

    inline Vector2 GetCenteredChunkPosition() const
    {
        return Vector2(
            m_chunk_info.m_position.x - (float(m_chunk_info.m_width) / 2.0f),
            m_chunk_info.m_position.y - (float(m_chunk_info.m_length) / 2.0f)
        );
    }

    ChunkInfo m_chunk_info;
    // std::shared_ptr<Entity> m_entity = nullptr;

    std::vector<double> m_heights; // TODO: refactor

protected:

    std::shared_ptr<Mesh> BuildMesh(const std::vector<double> &heights);
    void AddNormal(Vertex &vertex, const Vector3 &normal);
    void CalculateNormals(std::vector<Vertex> &vertices, const std::vector<MeshIndex> &indices);
    std::vector<Vertex> BuildVertices(const std::vector<double> &heights);
    std::vector<MeshIndex> BuildIndices();
};

} // namespace hyperion

#endif
