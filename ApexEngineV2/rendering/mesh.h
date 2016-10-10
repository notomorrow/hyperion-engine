#ifndef MESH_H
#define MESH_H

#include "renderable.h"
#include "vertex.h"

#include <vector>
#include <map>

namespace apex {
class Mesh : public Renderable {
public:
    enum PrimitiveType {
        PRIM_POINTS = 0x0000,
        PRIM_LINES,
        PRIM_LINE_LOOP,
        PRIM_LINE_STRIP,
        PRIM_TRIANGLES,
        PRIM_TRIANGLE_STRIP,
        PRIM_TRIANGLE_FAN
    };

    enum MeshAttributeType {
        ATTR_POSITIONS = 0x01,
        ATTR_NORMALS = 0x02,
        ATTR_TEXCOORDS0 = 0x04,
        ATTR_TEXCOORDS1 = 0x08,
        ATTR_TANGENTS = 0x10,
        ATTR_BITANGENTS = 0x20,
        ATTR_BONEWEIGHTS = 0x40,
        ATTR_BONEINDICES = 0x80,
    };

    struct MeshAttribute {
        static const MeshAttribute Positions, Normals,
            TexCoords0, TexCoords1,
            Tangents, Bitangents, 
            BoneWeights, BoneIndices;

        unsigned int offset, size, index;

        MeshAttribute()
        {
        }

        MeshAttribute(unsigned int offset, unsigned int size, unsigned int index)
            : offset(offset), size(size), index(index)
        {
        }

        MeshAttribute(const MeshAttribute &other)
            : offset(other.offset), size(other.size), index(other.index)
        {
        }

        bool operator==(const MeshAttribute &other) const
        {
            return offset == other.offset && 
                size == other.size && 
                index == other.index;
        }
    };

    Mesh();
    ~Mesh();

    void SetVertices(const std::vector<Vertex> &verts);
    void SetVertices(const std::vector<Vertex> &verts, const std::vector<uint32_t> &ind);
    inline const std::vector<Vertex> &GetVertices() const { return vertices; }
    inline const std::vector<uint32_t> &GetIndices() const { return indices; }

    void SetAttribute(MeshAttributeType type, const MeshAttribute &attribute);
    inline void SetPrimitiveType(PrimitiveType prim_type) { primitive_type = prim_type; }
    inline PrimitiveType GetPrimitiveType() const { return primitive_type; }

    void Render();

private:
    bool is_uploaded, is_created;
    unsigned int vbo, ibo, vertex_size;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    PrimitiveType primitive_type;

    // map attribute to offset
    std::map<MeshAttributeType, MeshAttribute> attribs;

    std::vector<float> CreateBuffer();
};
} // namespace apex

#endif