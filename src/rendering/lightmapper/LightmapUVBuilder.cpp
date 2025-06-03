/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapUVBuilder.hpp>
#include <rendering/RenderMesh.hpp>

#include <scene/Mesh.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion {

#pragma region LightmapUVMap

Bitmap<4, float> LightmapUVMap::ToBitmapRadiance() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<4, float> bitmap(width, height);

    for (uint32 x = 0; x < width; x++)
    {
        for (uint32 y = 0; y < height; y++)
        {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].radiance;

            if (color.w < 1.0f)
            {
                continue;
            }

            color /= color.w;

            bitmap.GetPixelAtIndex(index).SetRGBA(color);
        }
    }

    return bitmap;
}

Bitmap<4, float> LightmapUVMap::ToBitmapIrradiance() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<4, float> bitmap(width, height);

    for (uint32 x = 0; x < width; x++)
    {
        for (uint32 y = 0; y < height; y++)
        {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].irradiance;

            if (color.w < 1.0f)
            {
                continue;
            }

            color /= color.w;

            bitmap.GetPixelAtIndex(index).SetRGBA(color);
        }
    }

    return bitmap;
}

#pragma endregion LightmapUVMap

#pragma region LightmapUVBuilder

LightmapUVBuilder::LightmapUVBuilder(const LightmapUVBuilderParams& params)
    : m_params(params),
      m_mesh_vertex_positions(m_params.sub_elements.Size()),
      m_mesh_vertex_normals(m_params.sub_elements.Size()),
      m_mesh_vertex_uvs(m_params.sub_elements.Size()),
      m_mesh_indices(m_params.sub_elements.Size())
{
    // Output mesh data - this will be where we output the computed UVs to be used for tracing
    m_mesh_data.Resize(m_params.sub_elements.Size());

    for (SizeType i = 0; i < m_params.sub_elements.Size(); i++)
    {
        const LightmapSubElement& sub_element = m_params.sub_elements[i];

        LightmapMeshData& lightmap_mesh_data = m_mesh_data[i];

        if (!sub_element.mesh)
        {
            HYP_LOG(Lightmap, Warning, "Sub-element {} has no mesh, skipping", i);

            continue;
        }

        const Handle<Mesh>& mesh = sub_element.mesh;

        StreamedMeshData* streamed_mesh_data = mesh->GetStreamedMeshData();

        if (!streamed_mesh_data)
        {
            HYP_LOG(Lightmap, Error, "Sub-element {} has no streamed mesh data, skipping", i);

            continue;
        }

        TResourceHandle<StreamedMeshData> resource_handle(*streamed_mesh_data);

        if (!resource_handle)
        {
            return;
        }

        MeshData mesh_data = resource_handle->GetMeshData();

        HYP_LOG(Lightmap, Debug, "Resource handle has {} vertices and {} indices",
            mesh_data.vertices.Size(),
            mesh_data.indices.Size());

        lightmap_mesh_data.mesh = sub_element.mesh;
        lightmap_mesh_data.material = sub_element.material;
        lightmap_mesh_data.transform = sub_element.transform.GetMatrix();

        m_mesh_vertex_positions[i].Resize(mesh_data.vertices.Size() * 3);
        m_mesh_vertex_normals[i].Resize(mesh_data.vertices.Size() * 3);
        m_mesh_vertex_uvs[i].Resize(mesh_data.vertices.Size() * 2);
        m_mesh_indices[i] = mesh_data.indices;

        DebugLog(LogType::Debug, "Mesh index %u, indices (%llu, %llu, %u), vertices: %llu\n",
            i, m_mesh_indices[i].Size(), mesh_data.indices.Size(),
            streamed_mesh_data->NumIndices(),
            mesh_data.vertices.Size());

        AssertThrow(m_mesh_indices[i].Size() % 3 == 0);

        const Matrix4 normal_matrix = sub_element.transform.GetMatrix().Inverted().Transpose();

        for (SizeType vertex_index = 0; vertex_index < mesh_data.vertices.Size(); vertex_index++)
        {
            const Vec3f position = sub_element.transform.GetMatrix() * mesh_data.vertices[vertex_index].GetPosition();
            const Vec3f normal = (normal_matrix * Vec4f(mesh_data.vertices[vertex_index].GetNormal(), 0.0f)).GetXYZ().Normalize();
            const Vec2f uv = mesh_data.vertices[vertex_index].GetTexCoord0();

            m_mesh_vertex_positions[i][vertex_index * 3] = position.x;
            m_mesh_vertex_positions[i][vertex_index * 3 + 1] = position.y;
            m_mesh_vertex_positions[i][vertex_index * 3 + 2] = position.z;

            m_mesh_vertex_normals[i][vertex_index * 3] = normal.x;
            m_mesh_vertex_normals[i][vertex_index * 3 + 1] = normal.y;
            m_mesh_vertex_normals[i][vertex_index * 3 + 2] = normal.z;

            m_mesh_vertex_uvs[i][vertex_index * 2] = uv.x;
            m_mesh_vertex_uvs[i][vertex_index * 2 + 1] = uv.y;
        }
    }
}

TResult<LightmapUVMap> LightmapUVBuilder::Build()
{
    if (m_mesh_data.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No mesh data to build lightmap UVs from");
    }

    LightmapUVMap uv_map;

#ifdef HYP_XATLAS
    xatlas::Atlas* atlas = xatlas::Create();

    for (SizeType i = 0; i < m_mesh_data.Size(); i++)
    {
        xatlas::MeshDecl mesh_decl;
        mesh_decl.indexData = m_mesh_indices[i].Data();
        mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
        mesh_decl.indexCount = uint32(m_mesh_indices[i].Size());
        mesh_decl.vertexCount = uint32(m_mesh_vertex_positions[i].Size() / 3);
        mesh_decl.vertexPositionData = m_mesh_vertex_positions[i].Data();
        mesh_decl.vertexPositionStride = sizeof(float) * 3;
        mesh_decl.vertexNormalData = m_mesh_vertex_normals[i].Data();
        mesh_decl.vertexNormalStride = sizeof(float) * 3;
        mesh_decl.vertexUvData = m_mesh_vertex_uvs[i].Data();
        mesh_decl.vertexUvStride = sizeof(float) * 2;

        DebugLog(LogType::Debug, "Adding mesh %zu with %u vertices and %u indices\n",
            i, mesh_decl.vertexCount, mesh_decl.indexCount);

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);

        if (error != xatlas::AddMeshError::Success)
        {
            DebugLog(LogType::Error, "Error adding mesh: %s\n", xatlas::StringForEnum(error));

            xatlas::Destroy(atlas);

            return HYP_MAKE_ERROR(Error, "Error adding mesh");
        }

        xatlas::AddMeshJoin(atlas);
    }

    xatlas::PackOptions pack_options {};
    // pack_options.maxChartSize = 256;//2048;
    // pack_options.resolution = 1024; // testing
    // pack_options.padding = 8;
    // pack_options.texelsPerUnit = 128.0f;
    pack_options.bilinear = true;
    // pack_options.blockAlign = true;
    pack_options.bruteForce = true;
    pack_options.rotateCharts = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, pack_options);

    // write lightmap data
    uv_map.width = atlas->width;
    uv_map.height = atlas->height;
    uv_map.uvs.Resize(atlas->width * atlas->height);

    for (uint32 mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++)
    {
        AssertThrow(mesh_index < m_mesh_data.Size());

        const xatlas::Mesh& atlas_mesh = atlas->meshes[mesh_index];

        for (uint32 i = 0; i < atlas_mesh.indexCount; i += 3)
        {
            bool skip = false;
            int atlas_index = -1;
            FixedArray<Pair<uint32, Vec2i>, 3> verts;

            for (uint32 j = 0; j < 3; j++)
            {
                // Get UV coordinates for each edge
                const xatlas::Vertex& v = atlas_mesh.vertexArray[atlas_mesh.indexArray[i + j]];

                if (v.atlasIndex == -1)
                {
                    skip = true;

                    break;
                }

                atlas_index = v.atlasIndex;

                verts[j] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
            }

            if (skip)
            {
                continue;
            }

            const Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };

            const Vec2i clamp { int(uv_map.width - 1), int(uv_map.height - 1) };

            Vec2i bboxmin { int(uv_map.width - 1), int(uv_map.height - 1) };
            Vec2i bboxmax { 0, 0 };

            for (int j = 0; j < 3; j++)
            {
                bboxmin.x = MathUtil::Max(0, MathUtil::Min(bboxmin.x, pts[j].x));
                bboxmin.y = MathUtil::Max(0, MathUtil::Min(bboxmin.y, pts[j].y));

                bboxmax.x = MathUtil::Min(clamp.x, MathUtil::Max(bboxmax.x, pts[j].x));
                bboxmax.y = MathUtil::Min(clamp.y, MathUtil::Max(bboxmax.y, pts[j].y));
            }

            Vec2i point;

            for (point.x = bboxmin.x; point.x <= bboxmax.x; point.x++)
            {
                for (point.y = bboxmin.y; point.y <= bboxmax.y; point.y++)
                {
                    const Vec3f bc_screen = MathUtil::CalculateBarycentricCoordinates(Vec2f(pts[0]), Vec2f(pts[1]), Vec2f(pts[2]), Vec2f(point));

                    if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0)
                    {
                        continue;
                    }

                    const uint32 index = (point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width;

                    uv_map.uvs[index] = {
                        m_mesh_data[mesh_index].mesh,                                      // mesh
                        m_mesh_data[mesh_index].material,                                  // material
                        m_mesh_data[mesh_index].transform,                                 // transform
                        i / 3,                                                             // triangle_index
                        bc_screen,                                                         // barycentric_coords
                        Vec2f(point) / Vec2f { float(atlas->width), float(atlas->height) } // lightmap_uv
                    };

                    uv_map.mesh_to_uv_indices[m_mesh_data[mesh_index].mesh.GetID()].PushBack(index);
                }
            }
        }
    }

    for (SizeType mesh_index = 0; mesh_index < m_mesh_data.Size(); mesh_index++)
    {
        LightmapMeshData& lightmap_mesh_data = m_mesh_data[mesh_index];
        lightmap_mesh_data.vertices.Resize(atlas->meshes[mesh_index].vertexCount);
        lightmap_mesh_data.indices.Resize(atlas->meshes[mesh_index].indexCount);

        AssertThrow(m_mesh_indices[mesh_index].Size() == atlas->meshes[mesh_index].indexCount);

        for (uint32 j = 0; j < atlas->meshes[mesh_index].indexCount; j++)
        {
            // const uint32 index = atlas->meshes[mesh_index].indexArray[j];
            // const uint32 original_vertex_index = atlas->meshes[mesh_index].vertexArray[index].xref;

            // lightmap_mesh_data.vertex_lightmap_uvs[original_vertex_index * 2] = float(atlas->meshes[mesh_index].vertexArray[index].uv[0]) / float(atlas->width);
            // lightmap_mesh_data.vertex_lightmap_uvs[original_vertex_index * 2 + 1] = float(atlas->meshes[mesh_index].vertexArray[index].uv[1]) / float(atlas->height);

            lightmap_mesh_data.indices[j] = atlas->meshes[mesh_index].indexArray[j];

            const uint32 vertex_index = atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].xref;

            Vertex& vertex = lightmap_mesh_data.vertices[lightmap_mesh_data.indices[j]];

            vertex.SetPosition(Vec3f(
                m_mesh_vertex_positions[mesh_index][vertex_index * 3],
                m_mesh_vertex_positions[mesh_index][vertex_index * 3 + 1],
                m_mesh_vertex_positions[mesh_index][vertex_index * 3 + 2]));

            vertex.SetNormal(Vec3f(
                m_mesh_vertex_normals[mesh_index][vertex_index * 3],
                m_mesh_vertex_normals[mesh_index][vertex_index * 3 + 1],
                m_mesh_vertex_normals[mesh_index][vertex_index * 3 + 2]));

            vertex.SetTexCoord0(Vec2f(
                m_mesh_vertex_uvs[mesh_index][vertex_index * 2],
                m_mesh_vertex_uvs[mesh_index][vertex_index * 2 + 1]));

            vertex.SetTexCoord1(Vec2f(
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[0]) / float(atlas->width),
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[1]) / float(atlas->height)));
        }

        // Deallocate memory for data that is no longer needed.
        m_mesh_vertex_positions[mesh_index].Clear();
        m_mesh_vertex_positions[mesh_index].Refit();

        m_mesh_vertex_normals[mesh_index].Clear();
        m_mesh_vertex_normals[mesh_index].Refit();

        m_mesh_vertex_uvs[mesh_index].Clear();
        m_mesh_vertex_uvs[mesh_index].Refit();

        m_mesh_indices[mesh_index].Clear();
        m_mesh_indices[mesh_index].Refit();
    }

    xatlas::Destroy(atlas);

    return std::move(uv_map);
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

#pragma endregion LightmapUVBuilder

} // namespace hyperion