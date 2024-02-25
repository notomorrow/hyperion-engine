#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#define HYP_XATLAS

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion::v2 {

// LightmapUVMap

Bitmap<3> LightmapUVMap::ToBitmap() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<3> bitmap(width, height);

    for (uint x = 0; x < width; x++) {
        for (uint y = 0; y < height; y++) {
            const uint index = x + y * width;

            const Vec3f color = {
                uvs[index].color.x,
                uvs[index].color.y,
                uvs[index].color.z
            };

            bitmap.SetPixel(x, y, { ubyte(color.x * 255.0f), ubyte(color.y * 255.0f), ubyte(color.z * 255.0f) });
        }
    }

    return bitmap;
}


// LightmapUVBuilder

LightmapUVBuilder::LightmapUVBuilder(const LightmapUVBuilderParams &params)
    : m_params(params)
{
    m_mesh_data.Resize(params.elements.Size());

    for (SizeType i = 0; i < params.elements.Size(); i++) {
        const LightmapEntity &element = params.elements[i];

        LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];

        if (!element.mesh || !element.mesh->GetStreamedMeshData()) {
            continue;
        }

        const Handle<Mesh> &mesh = element.mesh;

        const RC<StreamedMeshData> &mesh_data = mesh->GetStreamedMeshData();
        AssertThrow(mesh_data != nullptr);

        auto ref = mesh_data->AcquireRef();

        lightmap_mesh_data.mesh_id = element.mesh.GetID();

        lightmap_mesh_data.transform = element.transform;

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
}


LightmapUVBuilder::Result LightmapUVBuilder::Build()
{
    if (!m_params.elements.Any()) {
        return { Result::RESULT_ERR, "No elements to build lightmap" };
    }

    LightmapUVMap uv_map;

#ifdef HYP_XATLAS
    xatlas::Atlas *atlas = xatlas::Create();

    for (SizeType i = 0; i < m_mesh_data.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];
    
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
    pack_options.resolution = 512; // temp
    //pack_options.padding = 2;
    //pack_options.blockAlign = true;
    //pack_options.rotateCharts = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, pack_options);
    
    // write lightmap data
    uv_map.width = atlas->width;
    uv_map.height = atlas->height;
    uv_map.uvs.Resize(atlas->width * atlas->height);

    for (uint mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++) {
        AssertThrow(mesh_index < m_mesh_data.Size());

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
            

            // AssertThrowMsg(atlas_mesh.indexArray[i] < lightmap_uvs.Size() / 2, "Index out of bounds: %d >= %u", atlas_mesh.indexArray[i], lightmap_uvs.Size() / 2);

            m_mesh_data[mesh_index].lightmap_uvs[verts[0].first * 2 + 0] = float(verts[0].second[0]) / float(atlas->width);
            m_mesh_data[mesh_index].lightmap_uvs[verts[0].first * 2 + 1] = float(verts[0].second[1]) / float(atlas->height);
            m_mesh_data[mesh_index].lightmap_uvs[verts[1].first * 2 + 0] = float(verts[1].second[0]) / float(atlas->width);
            m_mesh_data[mesh_index].lightmap_uvs[verts[1].first * 2 + 1] = float(verts[1].second[1]) / float(atlas->height);
            m_mesh_data[mesh_index].lightmap_uvs[verts[2].first * 2 + 0] = float(verts[2].second[0]) / float(atlas->width);
            m_mesh_data[mesh_index].lightmap_uvs[verts[2].first * 2 + 1] = float(verts[2].second[1]) / float(atlas->height);

            Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };

            Vec2i bboxmin(uv_map.width - 1,  uv_map.height - 1); 
            Vec2i bboxmax(0, 0); 
            Vec2i clamp(uv_map.width - 1, uv_map.height - 1); 
            for (int i=0; i<3; i++) { 
                bboxmin.x = MathUtil::Max(0, MathUtil::Min(bboxmin.x, pts[i].x));
	            bboxmin.y = MathUtil::Max(0, MathUtil::Min(bboxmin.y, pts[i].y));

	            bboxmax.x = MathUtil::Min(clamp.x, MathUtil::Max(bboxmax.x, pts[i].x));
	            bboxmax.y = MathUtil::Min(clamp.y, MathUtil::Max(bboxmax.y, pts[i].y));
            } 
            Vec2i P; 
            for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) { 
                for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) { 
                    Vec3f bc_screen  = MathUtil::CalculateBarycentricCoordinates(Vec2f(pts[0]), Vec2f(pts[1]), Vec2f(pts[2]), Vec2f(P));
                    if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;

                    const Vec2f lightmap_uv = Vec2f { float(P.x) / float(atlas->width), float(P.y) / float(atlas->height) };

                    const uint index = (P.x + atlas->width) % atlas->width
                        + (atlas->height - P.y + atlas->height) % atlas->height * atlas->width;

                    uv_map.uvs[index] = {
                        m_mesh_data[mesh_index].mesh_id,    // mesh_id
                        m_mesh_data[mesh_index].transform,  // transform
                        i / 3,                              // triangle_index
                        bc_screen,                          // barycentric_coords
                        lightmap_uv                         // lightmap_uv
                    };

                    auto mesh_to_uv_indices_it = uv_map.mesh_to_uv_indices.Find(m_mesh_data[mesh_index].mesh_id);

                    if (mesh_to_uv_indices_it == uv_map.mesh_to_uv_indices.End()) {
                        mesh_to_uv_indices_it = uv_map.mesh_to_uv_indices.Insert(m_mesh_data[mesh_index].mesh_id, { }).first;
                    }

                    mesh_to_uv_indices_it->second.PushBack(index);
                } 
            } 

            //Vec2i t0 = verts[0].second;
            //Vec2i t1 = verts[1].second;
            //Vec2i t2 = verts[2].second;

            //if (t0.y > t1.y) {
            //    std::swap(t0, t1);
            //}

            //if (t0.y > t2.y) {
            //    std::swap(t0, t2);
            //}

            //if (t1.y > t2.y) {
            //    std::swap(t1, t2);
            //}

            //int total_height = t2.y - t0.y;

            //if (total_height == 0) {
            //    continue;
            //}

            //for (int y = t0.y; y <= t1.y; y++) {
            //    int segment_height = t1.y - t0.y + 1; 
            //    float alpha = (float)(y - t0.y) / total_height;
            //    float beta = (float)(y - t0.y) / segment_height;

            //    Vec2i A = t0 + (t2 - t0) * alpha;
            //    Vec2i B = t0 + (t1 - t0) * beta;

            //    if (A.x > B.x) {
            //        std::swap(A, B);
            //    }

            //    for (int x = A.x; x <= B.x; x++) {
            //        Vec3f u = 

            //        const Vec2f lightmap_uv = Vec2f { float(x) / float(atlas->width), float(y) / float(atlas->height) };
            //        const Vec3f barycentric_coords = Vec3f { 1.0f - (float(x - t0.x) / (t2.x - t0.x)), 1.0f - (float(y - t0.y) / (t2.y - t0.y)), 1.0f - (float(x - t1.x) / (t2.x - t1.x)) };

            //        const uint index = (x + atlas->width) % atlas->width
            //            + (atlas->height - y + atlas->height) % atlas->height * atlas->width;

            //        uv_map.uvs[index] = {
            //            lightmap_mesh_datas[mesh_index].mesh_id,    // mesh_id
            //            i / 3,                                      // triangle_index
            //            barycentric_coords,                         // barycentric_coords (TODO) 
            //            lightmap_uv                                 // lightmap_uv
            //        };
            //    }
            //}

            //for (int y = t1.y; y <= t2.y; y++) {
            //    int segment_height = t2.y - t1.y + 1;
            //    float alpha = (float)(y - t0.y) / total_height;
            //    float beta = (float)(y - t1.y) / segment_height;

            //    Vec2i A = t0 + (t2 - t0) * alpha;
            //    Vec2i B = t1 + (t2 - t1) * beta;

            //    if (A.x > B.x) {
            //        std::swap(A, B);
            //    }

            //    for (int x = A.x; x <= B.x; x++) {
            //        const Vec2f lightmap_uv = Vec2f { float(x) / float(atlas->width), float(y) / float(atlas->height) };
            //        const Vec3f barycentric_coords = Vec3f { 1.0f - (float(x - t0.x) / (t2.x - t0.x)), 1.0f - (float(y - t0.y) / (t2.y - t0.y)), 1.0f - (float(x - t1.x) / (t2.x - t1.x)) };

            //        const uint index = (x + atlas->width) % atlas->width
            //            + (atlas->height - y + atlas->height) % atlas->height * atlas->width;

            //        uv_map.uvs[index] = {
            //            lightmap_mesh_datas[mesh_index].mesh_id,    // mesh_id
            //            i / 3,                                      // triangle_index
            //            barycentric_coords,                         // barycentric_coords (TODO) 
            //            lightmap_uv                                 // lightmap_uv
            //        };
            //    }
            //}
        }
    }

    xatlas::Destroy(atlas);

    for (SizeType i = 0; i < m_mesh_data.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];
        const LightmapEntity &element = m_params.elements[i];

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

        Mesh::SetStreamedMeshData(mesh, StreamedMeshData::FromMeshData(new_mesh_data));
    }

    return {
        LightmapUVBuilder::Result::RESULT_OK,
        std::move(uv_map)
    };
#else
    return { LightmapUVBuilder::Result::RESULT_ERR, "No method to build lightmap" };
#endif
}

} // namespace hyperion::v2