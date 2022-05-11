#ifndef VERTEX_H
#define VERTEX_H

#define MAX_BONE_WEIGHTS 4
#define MAX_BONE_INDICES 4

#include "vector2.h"
#include "vector3.h"
#include "transform.h"
#include "matrix4.h"
#include <hash_code.h>
#include <util/defines.h>

#include <array>
#include <cstring>

namespace hyperion {

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
          nboneweights(other.nboneweights),
          bone_weights(other.bone_weights),
          bone_indices(other.bone_indices)
    {
    }
    
    bool operator<(const Vertex &other) const;
    bool operator==(const Vertex &other) const;
    Vertex &operator=(const Vertex &other);
    Vertex operator*(float scalar) const;
    Vertex operator*(const Matrix4 &mat) const;
    Vertex operator*(const Transform &transform) const;
    Vertex &operator*=(float scalar);
    Vertex &operator*=(const Matrix4 &mat);
    Vertex &operator*=(const Transform &transform);

    inline void SetPosition(const Vector3 &vec) { position = vec; }
    inline const Vector3 &GetPosition() const { return position; }
    inline void SetNormal(const Vector3 &vec) { normal = vec; }
    inline const Vector3 &GetNormal() const { return normal; }
    inline void SetTexCoord0(const Vector2 &vec) { texcoord0 = vec; }
    inline const Vector2 &GetTexCoord0() const { return texcoord0; }
    inline void SetTexCoord1(const Vector2 &vec) { texcoord1 = vec; }
    inline const Vector2 &GetTexCoord1() const { return texcoord1; }
    inline void SetTangent(const Vector3 &vec) { tangent = vec; }
    inline const Vector3 &GetTangent() const { return tangent; }
    inline void SetBitangent(const Vector3 &vec) { bitangent = vec; }
    inline const Vector3 &GetBitangent() const { return bitangent; }

    inline void SetBoneWeight(int i, float val) { bone_weights[i] = val; }
    inline float GetBoneWeight(int i) const { return bone_weights[i]; }
    inline void SetBoneIndex(int i, int val) { bone_indices[i] = val; }
    inline int GetBoneIndex(int i) const { return bone_indices[i]; }

    inline void AddBoneWeight(float val) { if (nboneweights < MAX_BONE_WEIGHTS) bone_weights[nboneweights++] = val; }
    inline void AddBoneIndex(int val)    { if (nboneindices < MAX_BONE_INDICES) bone_indices[nboneindices++] = val; }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(texcoord0.GetHashCode());
        hc.Add(texcoord1.GetHashCode());
        hc.Add(tangent.GetHashCode());
        hc.Add(bitangent.GetHashCode());
        hc.Add(nboneindices);
        hc.Add(nboneweights);

        for (uint32_t i = 0; i < MAX_BONE_INDICES; i++) {
            hc.Add(bone_indices[i]);
        }

        for (uint32_t i = 0; i < MAX_BONE_WEIGHTS; i++) {
            hc.Add(bone_weights[i]);
        }

        return hc;
    }

private:
    Vector3 position;
    Vector3 normal;
    Vector2 texcoord0;
    Vector2 texcoord1;
    Vector3 tangent;
    Vector3 bitangent;

    int nboneindices,
        nboneweights;

    std::array<float, MAX_BONE_WEIGHTS> bone_weights;
    std::array<int, MAX_BONE_INDICES> bone_indices;
};

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vertex);

#endif
