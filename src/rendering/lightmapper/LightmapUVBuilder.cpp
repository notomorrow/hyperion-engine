#include <rendering/lightmapper/LightmapUVBuilder.hpp>
#include <util/img/Bitmap.hpp>

#define HYP_XATLAS

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion::v2 {

struct LightmapMeshData
{
    Array<float>    vertex_positions;
    Array<float>    vertex_normals;
    Array<float>    vertex_uvs;

    Array<uint32>   indices;

    Array<float>    lightmap_uvs;
};

LightmapUVBuilder::Result LightmapUVBuilder::Build(const LightmapUVBuilderParams &params)
{
    if (!params.elements.Any()) {
        return { LightmapUVBuilder::Result::RESULT_ERR, "No elements to build lightmap" };
    }

    Array<LightmapMeshData> lightmap_mesh_datas;
    lightmap_mesh_datas.Resize(params.elements.Size());

    for (SizeType i = 0; i < params.elements.Size(); i++) {
        const LightmapUVBuilderParams::Element &element = params.elements[i];

        LightmapMeshData &lightmap_mesh_data = lightmap_mesh_datas[i];

        if (!element.mesh) {
            return { LightmapUVBuilder::Result::RESULT_ERR, "Element has no mesh" };
        }

        if (!element.mesh->NumIndices()) {
            return { LightmapUVBuilder::Result::RESULT_ERR, "Mesh has no indices" };
        }

        const Handle<Mesh> &mesh = element.mesh;

        const RC<StreamedMeshData> &mesh_data = mesh->GetStreamedMeshData();
        AssertThrow(mesh_data != nullptr);

        auto ref = mesh_data->AcquireRef();

        lightmap_mesh_data.vertex_positions.Resize(ref->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_normals.Resize(ref->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_uvs.Resize(ref->GetMeshData().vertices.Size() * 2);

        lightmap_mesh_data.indices = ref->GetMeshData().indices;

        lightmap_mesh_data.lightmap_uvs.Resize(ref->GetMeshData().vertices.Size() * 2);

        for (SizeType i = 0; i < ref->GetMeshData().vertices.Size(); i++) {
            lightmap_mesh_data.vertex_positions[i * 3] = ref->GetMeshData().vertices[i].GetPosition().x;
            lightmap_mesh_data.vertex_positions[i * 3 + 1] = ref->GetMeshData().vertices[i].GetPosition().y;
            lightmap_mesh_data.vertex_positions[i * 3 + 2] = ref->GetMeshData().vertices[i].GetPosition().z;

            lightmap_mesh_data.vertex_normals[i * 3] = ref->GetMeshData().vertices[i].GetNormal().x;
            lightmap_mesh_data.vertex_normals[i * 3 + 1] = ref->GetMeshData().vertices[i].GetNormal().y;
            lightmap_mesh_data.vertex_normals[i * 3 + 2] = ref->GetMeshData().vertices[i].GetNormal().z;

            lightmap_mesh_data.vertex_uvs[i * 2] = ref->GetMeshData().vertices[i].GetTexCoord0().x;
            lightmap_mesh_data.vertex_uvs[i * 2 + 1] = ref->GetMeshData().vertices[i].GetTexCoord0().y;
        }
    }

#ifdef HYP_XATLAS
    xatlas::Atlas *atlas = xatlas::Create();

    for (SizeType i = 0; i < lightmap_mesh_datas.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = lightmap_mesh_datas[i];
    
        xatlas::MeshDecl mesh_decl;
        mesh_decl.indexData = lightmap_mesh_data.indices.Data();
        mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
        mesh_decl.indexCount = uint32(lightmap_mesh_data.indices.Size());
        mesh_decl.vertexCount = uint32(lightmap_mesh_data.vertex_positions.Size() / 3);
        mesh_decl.vertexPositionData = lightmap_mesh_data.vertex_positions.Data();
        mesh_decl.vertexPositionStride = sizeof(float) * 3;
        mesh_decl.vertexNormalData = lightmap_mesh_data.vertex_normals.Data();
        mesh_decl.vertexNormalStride = sizeof(float) * 3;
        mesh_decl.vertexUvData = lightmap_mesh_data.vertex_uvs.Data();
        mesh_decl.vertexUvStride = sizeof(float) * 2;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);

        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);

            DebugLog(LogType::Error, "Error adding mesh: %s\n", xatlas::StringForEnum(error));

            return { LightmapUVBuilder::Result::RESULT_ERR, "Error adding mesh" };
        }
    }

    xatlas::PackOptions pack_options { };
    pack_options.resolution = 2048;
    pack_options.padding = 2;
    pack_options.blockAlign = 1;
    pack_options.rotateCharts = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, pack_options);
    
    // write lightmap data
    Bitmap<3> bitmap { atlas->width, atlas->height };

    for (uint mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++) {
        AssertThrow(mesh_index < lightmap_mesh_datas.Size());

        const xatlas::Mesh &atlas_mesh = atlas->meshes[mesh_index];

        for (uint i = 0; i < atlas_mesh.indexCount; i += 3) {
            bool skip = false;
            int atlas_index = -1;
            FixedArray<Pair<uint, Vec2i>, 3> verts;

            for (uint j = 0; j < 3; j++) {
                // Get UV coordinates for each edge
                const xatlas::Vertex &v = atlas_mesh.vertexArray[atlas_mesh.indexArray[i + j]];

                if (v.atlasIndex == -1) {
                    skip = true;

                    break;
                }
                
                atlas_index = v.atlasIndex;

                verts[j] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
            }

            if (skip) {
                continue;
            }
            
            ubyte random_color[3] = { ubyte(rand() % 256), ubyte(rand() % 256), ubyte(rand() % 256) };


            // AssertThrowMsg(atlas_mesh.indexArray[i] < lightmap_uvs.Size() / 2, "Index out of bounds: %d >= %u", atlas_mesh.indexArray[i], lightmap_uvs.Size() / 2);

            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[0].first * 2 + 0] = float(verts[0].second[0]) / float(atlas->width);
            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[0].first * 2 + 1] = float(verts[0].second[1]) / float(atlas->height);
            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[1].first * 2 + 0] = float(verts[1].second[0]) / float(atlas->width);
            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[1].first * 2 + 1] = float(verts[1].second[1]) / float(atlas->height);
            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[2].first * 2 + 0] = float(verts[2].second[0]) / float(atlas->width);
            lightmap_mesh_datas[mesh_index].lightmap_uvs[verts[2].first * 2 + 1] = float(verts[2].second[1]) / float(atlas->height);

            bitmap.FillTriangle((verts[0].second), (verts[1].second), (verts[2].second), { random_color[0], random_color[1], random_color[2] });
        }
    }

    const Name lightmap_name = Name::Unique("lightmap");

    bitmap.Write(String(lightmap_name.LookupString()) + ".bmp");

    xatlas::Destroy(atlas);

    for (SizeType i = 0; i < lightmap_mesh_datas.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = lightmap_mesh_datas[i];
        const LightmapUVBuilderParams::Element &element = params.elements[i];

        const Handle<Mesh> &mesh = element.mesh;
        AssertThrow(mesh.IsValid());

        auto ref = mesh->GetStreamedMeshData()->AcquireRef();

        MeshData new_mesh_data;
        new_mesh_data.vertices = ref->GetMeshData().vertices;
        new_mesh_data.indices = ref->GetMeshData().indices;

        for (SizeType i = 0; i < new_mesh_data.vertices.Size(); i++) {
            Vec2f lightmap_uv = Vec2f(lightmap_mesh_data.lightmap_uvs[i * 2], lightmap_mesh_data.lightmap_uvs[i * 2 + 1]);

            new_mesh_data.vertices[i].SetTexCoord1(lightmap_uv);
        }

        mesh->SetStreamedMeshData(StreamedMeshData::FromMeshData(new_mesh_data));
        mesh->SetVertexAttributes(mesh->GetVertexAttributes() | renderer::VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1);
    }

    return {
        LightmapUVBuilder::Result::RESULT_OK,
        Lightmap {
            lightmap_name,
            std::move(bitmap)
        }
    };
#else
    return { LightmapUVBuilder::Result::RESULT_ERR, "No method to build lightmap" };
#endif
}

} // namespace hyperion::v2