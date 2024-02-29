#ifndef VERTEX_H
#define VERTEX_H

#define MAX_BONE_WEIGHTS 4
#define MAX_BONE_INDICES 4

#include <core/lib/FixedArray.hpp>
#include <core/lib/Variant.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Transform.hpp>
#include <math/Matrix4.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct alignas(16) Vertex
{
    friend Vertex operator*(const Matrix4 &mat, const Vertex &vertex);
    friend Vertex operator*(const Transform &transform, const Vertex &vertex);

    Vertex()
        : num_indices(0),
          num_weights(0)
    {
        for (uint i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position)
        : position(position),
          num_indices(0),
          num_weights(0)
    {
        for (uint i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position, const Vector2 &texcoord0)
        : position(position),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (uint i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position, const Vector2 &texcoord0, const Vector3 &normal)
        : position(position),
          normal(normal),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (uint i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vertex &other)
        : position(other.position),
          normal(other.normal),
          texcoord0(other.texcoord0),
          texcoord1(other.texcoord1),
          tangent(other.tangent),
          bitangent(other.bitangent),
          num_indices(other.num_indices),
          num_weights(other.num_weights),
          bone_weights(other.bone_weights),
          bone_indices(other.bone_indices)
    {
    }
    
    bool operator==(const Vertex &other) const;
    // Vertex &operator=(const Vertex &other);
    Vertex operator*(float scalar) const;
    Vertex &operator*=(float scalar);

    void SetPosition(const Vector3 &vec)  { position = vec; }
    const Vector3 &GetPosition() const    { return position; }
    void SetNormal(const Vector3 &vec)    { normal = vec; }
    const Vector3 &GetNormal() const      { return normal; }
    void SetTexCoord0(const Vector2 &vec) { texcoord0 = vec; }
    const Vector2 &GetTexCoord0() const   { return texcoord0; }
    void SetTexCoord1(const Vector2 &vec) { texcoord1 = vec; }
    const Vector2 &GetTexCoord1() const   { return texcoord1; }
    void SetTangent(const Vector3 &vec)   { tangent = vec; }
    const Vector3 &GetTangent() const     { return tangent; }
    void SetBitangent(const Vector3 &vec) { bitangent = vec; }
    const Vector3 &GetBitangent() const   { return bitangent; }

    void SetBoneWeight(int i, float val) { bone_weights[i] = val; }
    float GetBoneWeight(int i) const     { return bone_weights[i]; }
    void SetBoneIndex(int i, int val)    { bone_indices[i] = val; }
    int GetBoneIndex(int i) const        { return bone_indices[i]; }

    void AddBoneWeight(float val) { if (num_weights < MAX_BONE_WEIGHTS) bone_weights[num_weights++] = val; }
    void AddBoneIndex(int val)    { if (num_indices < MAX_BONE_INDICES) bone_indices[num_indices++] = val; }

    /*! \brief Read the attribute from the vertex into \ref{ptr}. The value at \ref{ptr} must be able to hold sizeof(float) * 4.
     *  If an invalid attribute is passed, the function does nothing.
     *
     *  \param attr The attribute to read.
     *  \param ptr The pointer to write the attribute to.
     */
    void ReadAttribute(renderer::VertexAttribute::Type attr, void *ptr) const
    {
        switch (attr) {
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION:
            Memory::MemCpy(ptr, &position, sizeof(float) * 3);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL:
            Memory::MemCpy(ptr, &normal, sizeof(float) * 3);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT:
            Memory::MemCpy(ptr, &tangent, sizeof(float) * 3);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT:
            Memory::MemCpy(ptr, &bitangent, sizeof(float) * 3);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0:
            Memory::MemCpy(ptr, &texcoord0, sizeof(float) * 2);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1:
            Memory::MemCpy(ptr, &texcoord1, sizeof(float) * 2);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES:
            Memory::MemCpy(ptr, bone_indices.Data(), sizeof(uint) * MAX_BONE_INDICES);
            break;
        case renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS:
            Memory::MemCpy(ptr, bone_weights.Data(), sizeof(float) * MAX_BONE_WEIGHTS);
            break;
        default:
            // Do nothing
            break;
        }
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(texcoord0.GetHashCode());
        hc.Add(texcoord1.GetHashCode());
        hc.Add(tangent.GetHashCode());
        hc.Add(bitangent.GetHashCode());
        hc.Add(num_indices);
        hc.Add(num_weights);

        for (uint i = 0; i < MAX_BONE_INDICES; i++) {
            hc.Add(bone_indices[i]);
        }

        for (uint i = 0; i < MAX_BONE_WEIGHTS; i++) {
            hc.Add(bone_weights[i]);
        }

        return hc;
    }

    Vec3f                               position;
    Vec3f                               normal;
    Vec3f                               tangent;
    Vec3f                               bitangent;
    Vec2f                               texcoord0;
    Vec2f                               texcoord1;

    FixedArray<float, MAX_BONE_WEIGHTS> bone_weights;
    FixedArray<uint, MAX_BONE_INDICES>  bone_indices;

    uint8                               num_indices;
    uint8                               num_weights;
};

Vertex operator*(const Matrix4 &mat, const Vertex &vertex);
Vertex operator*(const Transform &transform, const Vertex &vertex);

static_assert(sizeof(Vertex) == 128, "Vertex size is not 128 bytes, ensure size matches C# Vertex struct size");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vertex);

#endif
