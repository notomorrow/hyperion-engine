/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapUVBuilder.hpp>
#include <rendering/RenderMesh.hpp>

#include <scene/Mesh.hpp>

#include <streaming/StreamedMeshData.hpp>

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
    : m_params(params)
{
}

TResult<LightmapUVMap> LightmapUVBuilder::Build()
{
    if (!m_params.sub_elements)
    {
        return HYP_MAKE_ERROR(Error, "No subelements to build lightmap");
    }

    // Output mesh data - this will be where we output the computed UVs to be used for tracing
    m_mesh_data.Resize(m_params.sub_elements.Size());

    // Per element mesh data used for building the UV map
    Array<Array<float, DynamicAllocator>> mesh_vertex_positions(m_params.sub_elements.Size());
    Array<Array<float, DynamicAllocator>> mesh_vertex_normals(m_params.sub_elements.Size());
    Array<Array<float, DynamicAllocator>> mesh_vertex_uvs(m_params.sub_elements.Size());
    Array<Array<uint32, DynamicAllocator>> mesh_indices(m_params.sub_elements.Size());

    for (SizeType i = 0; i < m_params.sub_elements.Size(); i++)
    {
        const LightmapSubElement& sub_element = m_params.sub_elements[i];

        LightmapMeshData& lightmap_mesh_data = m_mesh_data[i];

        if (!sub_element.mesh || !sub_element.mesh->GetStreamedMeshData())
        {
            continue;
        }

        const Handle<Mesh>& mesh = sub_element.mesh;

        StreamedMeshData* streamed_mesh_data = mesh->GetStreamedMeshData();
        AssertThrow(streamed_mesh_data != nullptr);

        ResourceHandle resource_handle(*streamed_mesh_data);

        lightmap_mesh_data.mesh = sub_element.mesh;
        lightmap_mesh_data.material = sub_element.material;
        lightmap_mesh_data.transform = sub_element.transform.GetMatrix();

        mesh_vertex_positions[i].Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 3);
        mesh_vertex_normals[i].Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 3);
        mesh_vertex_uvs[i].Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 2);
        mesh_indices[i] = streamed_mesh_data->GetMeshData().indices;

        const Matrix4 normal_matrix = sub_element.transform.GetMatrix().Inverted().Transpose();

        for (SizeType j = 0; j < streamed_mesh_data->GetMeshData().vertices.Size(); j++)
        {
            const Vec3f position = sub_element.transform.GetMatrix() * streamed_mesh_data->GetMeshData().vertices[j].GetPosition();
            const Vec3f normal = (normal_matrix * Vec4f(streamed_mesh_data->GetMeshData().vertices[j].GetNormal(), 0.0f)).GetXYZ().Normalize();
            const Vec2f uv = streamed_mesh_data->GetMeshData().vertices[j].GetTexCoord0();

            mesh_vertex_positions[i][j * 3] = position.x;
            mesh_vertex_positions[i][j * 3 + 1] = position.y;
            mesh_vertex_positions[i][j * 3 + 2] = position.z;

            mesh_vertex_normals[i][j * 3] = normal.x;
            mesh_vertex_normals[i][j * 3 + 1] = normal.y;
            mesh_vertex_normals[i][j * 3 + 2] = normal.z;

            mesh_vertex_uvs[i][j * 2] = uv.x;
            mesh_vertex_uvs[i][j * 2 + 1] = uv.y;
        }
    }

    LightmapUVMap uv_map;

#ifdef HYP_XATLAS
    xatlas::Atlas* atlas = xatlas::Create();

    for (SizeType i = 0; i < m_mesh_data.Size(); i++)
    {
        xatlas::MeshDecl mesh_decl;
        mesh_decl.indexData = mesh_indices[i].Data();
        mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
        mesh_decl.indexCount = uint32(mesh_indices[i].Size());
        mesh_decl.vertexCount = uint32(mesh_vertex_positions[i].Size() / 3);
        mesh_decl.vertexPositionData = mesh_vertex_positions[i].Data();
        mesh_decl.vertexPositionStride = sizeof(float) * 3;
        mesh_decl.vertexNormalData = mesh_vertex_normals[i].Data();
        mesh_decl.vertexNormalStride = sizeof(float) * 3;
        mesh_decl.vertexUvData = mesh_vertex_uvs[i].Data();
        mesh_decl.vertexUvStride = sizeof(float) * 2;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);

        if (error != xatlas::AddMeshError::Success)
        {
            xatlas::Destroy(atlas);

            DebugLog(LogType::Error, "Error adding mesh: %s\n", xatlas::StringForEnum(error));

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
        lightmap_mesh_data.vertex_lightmap_uvs.Resize(mesh_vertex_positions[mesh_index].Size() / 3 * 2);

        AssertThrow(mesh_indices[mesh_index].Size() == atlas->meshes[mesh_index].indexCount);

        for (uint32 j = 0; j < atlas->meshes[mesh_index].indexCount; j++)
        {
            const uint32 index = atlas->meshes[mesh_index].indexArray[j];
            const uint32 original_vertex_index = atlas->meshes[mesh_index].vertexArray[index].xref;

            lightmap_mesh_data.vertex_lightmap_uvs[original_vertex_index * 2] = float(atlas->meshes[mesh_index].vertexArray[index].uv[0]) / float(atlas->width);
            lightmap_mesh_data.vertex_lightmap_uvs[original_vertex_index * 2 + 1] = float(atlas->meshes[mesh_index].vertexArray[index].uv[1]) / float(atlas->height);
        }

        // Deallocate memory for data that is no longer needed.
        mesh_vertex_positions[mesh_index].Clear();
        mesh_vertex_positions[mesh_index].Refit();

        mesh_vertex_normals[mesh_index].Clear();
        mesh_vertex_normals[mesh_index].Refit();

        mesh_vertex_uvs[mesh_index].Clear();
        mesh_vertex_uvs[mesh_index].Refit();

        mesh_indices[mesh_index].Clear();
        mesh_indices[mesh_index].Refit();
    }

    xatlas::Destroy(atlas);

    return std::move(uv_map);
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

#pragma endregion LightmapUVBuilder

} // namespace hyperion