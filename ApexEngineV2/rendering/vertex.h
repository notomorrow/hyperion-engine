#ifndef VERTEX_H
#define VERTEX_H

#define MAX_BONE_WEIGHTS 4
#define MAX_BONE_INDICES 4

#include "../math/vector2.h"
#include "../math/vector3.h"

namespace apex {
class Vertex {
public:
    Vertex()
        : nboneindices(0),
          nboneweights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position)
        : position(position),
          nboneindices(0),
          nboneweights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position, const Vector2 &texcoord0)
        : position(position),
          texcoord0(texcoord0),
          nboneindices(0),
          nboneweights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vector3 &position, const Vector2 &texcoord0, const Vector3 &normal)
        : position(position),
          texcoord0(texcoord0),
          normal(normal),
          nboneindices(0),
          nboneweights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
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
          nboneindices(other.nboneindices),
          nboneweights(other.nboneweights)
    {
        for (int i = 0; i < 4; i++) {
            bone_weights[i] = other.bone_weights[i];
            bone_indices[i] = other.bone_indices[i];
        }
    }

    void SetPosition(const Vector3 &vec)
    {
        position = vec;
    }

    const Vector3 &GetPosition() const
    {
        return position;
    }

    void SetNormal(const Vector3 &vec)
    {
        normal = vec;
    }

    const Vector3 &GetNormal() const
    {
        return normal;
    }

    void SetTexCoord0(const Vector2 &vec)
    {
        texcoord0 = vec;
    }

    const Vector2 &GetTexCoord0() const
    {
        return texcoord0;
    }

    void SetTexCoord1(const Vector2 &vec)
    {
        texcoord1 = vec;
    }

    const Vector2 &GetTexCoord1() const
    {
        return texcoord1;
    }

    void SetTangent(const Vector3 &vec)
    {
        tangent = vec;
    }

    const Vector3 &GetTangent() const
    {
        return tangent;
    }

    void SetBitangent(const Vector3 &vec)
    {
        bitangent = vec;
    }

    const Vector3 &GetBitangent() const
    {
        return bitangent;
    }

    void SetBoneWeight(size_t i, float val)
    {
        bone_weights[i] = val;
    }

    float GetBoneWeight(size_t i) const
    {
        return bone_weights[i];
    }

    void SetBoneIndex(size_t i, size_t val)
    {
        bone_indices[i] = val;
    }

    size_t GetBoneIndex(size_t i) const
    {
        return bone_indices[i];
    }

    void AddBoneWeight(float weight)
    {
        if (nboneweights < MAX_BONE_WEIGHTS) {
            bone_weights[nboneweights++] = weight;
        }
    }

    void AddBoneIndex(size_t index)
    {
        if (nboneindices < MAX_BONE_INDICES) {
            bone_indices[nboneindices++] = index;
        }
    }

private:
    size_t nboneindices,
        nboneweights;

    Vector3 position,
        normal,
        tangent,
        bitangent;

    Vector2 texcoord0,
        texcoord1;

    float bone_weights[4];
    size_t bone_indices[4];
};
}

#endif