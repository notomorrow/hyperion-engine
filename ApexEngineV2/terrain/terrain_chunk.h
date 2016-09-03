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
    HeightInfo height_info;
    std::shared_ptr<Entity> entity = nullptr;

    TerrainChunk(const HeightInfo &height_info);
    virtual ~TerrainChunk() = default;

protected:
    // to be implemented by derived class, as the way of generating heights
    virtual int HeightIndexAt(int x, int z) = 0;

    std::shared_ptr<Mesh> BuildMesh(const std::vector<double> &heights);
    void AddNormal(Vertex &vertex, const Vector3 &normal);
    void NormalizeNormal(Vertex &vertex);
    void CalculateNormals(std::vector<Vertex> &vertices, const std::vector<size_t> &indices);
    std::vector<Vertex> BuildVertices(const std::vector<double> &heights);
    std::vector<size_t> BuildIndices();
};
}

#endif