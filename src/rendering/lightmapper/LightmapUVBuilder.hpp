/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_UV_BUILDER_HPP
#define HYPERION_LIGHTMAP_UV_BUILDER_HPP

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/Handle.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

class Mesh;
class Material;
class Entity;
struct Vertex;

struct LightmapSubElement
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Transform transform;
    BoundingBox aabb;
};

struct LightmapUVBuilderParams
{
    Span<const LightmapSubElement> sub_elements;
};

struct LightmapMeshData
{
    Handle<Mesh> mesh;
    Handle<Material> material;

    Matrix4 transform;

    Array<Vertex> vertices;
    Array<uint32> indices;
};

struct LightmapUV
{
    Handle<Mesh> mesh;
    Handle<Material> material;
    Matrix4 transform = Matrix4::identity;
    uint32 triangle_index = ~0u;
    Vec3f barycentric_coords = Vec3f::Zero();
    Vec2f lightmap_uv = Vec2f::Zero();
    Vec4f radiance = Vec4f::Zero();
    Vec4f irradiance = Vec4f::Zero();
};

struct LightmapUVMap
{
    // HashMap from mesh ID to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = HashMap<ID<Mesh>, Array<uint32, DynamicAllocator>, HashTable_DynamicNodeAllocator<KeyValuePair<ID<Mesh>, Array<uint32, DynamicAllocator>>>>;

    uint32 width = 0;
    uint32 height = 0;

    /// UVs in texture space with each entry corresponding to a texel in the lightmap.
    Array<LightmapUV> uvs;

    // Mapping from mesh ID to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap mesh_to_uv_indices;

    /*! \brief Write the UV map radiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapRadiance() const;
    /*! \brief Write the UV map irradiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapIrradiance() const;
};

class LightmapUVBuilder
{
public:
    LightmapUVBuilder() = default;

    LightmapUVBuilder(const LightmapUVBuilderParams& params);

    LightmapUVBuilder(const LightmapUVBuilder& other)
        : m_params(other.m_params),
          m_mesh_data(other.m_mesh_data)
    {
    }

    LightmapUVBuilder(LightmapUVBuilder&& other) noexcept
        : m_params(std::move(other.m_params)),
          m_mesh_data(std::move(other.m_mesh_data))
    {
    }

    LightmapUVBuilder& operator=(const LightmapUVBuilder& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_params = other.m_params;
        m_mesh_data = other.m_mesh_data;

        return *this;
    }

    LightmapUVBuilder& operator=(LightmapUVBuilder&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_params = std::move(other.m_params);
        m_mesh_data = std::move(other.m_mesh_data);

        return *this;
    }

    ~LightmapUVBuilder() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData>& GetMeshData() const
    {
        return m_mesh_data;
    }

    TResult<LightmapUVMap> Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_mesh_data;
    // Per element mesh data used for building the UV map
    Array<Array<float>> m_mesh_vertex_positions;
    Array<Array<float>> m_mesh_vertex_normals;
    Array<Array<float>> m_mesh_vertex_uvs;
    Array<Array<uint32>> m_mesh_indices;
};

} // namespace hyperion

#endif