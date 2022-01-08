#include "bounding_box_renderer.h"
#include "../shaders/shader_code.h"

namespace apex {
const std::vector<MeshIndex> BoundingBoxRenderer::indices = {
    0, 1, 1, 2, 2, 3,
    3, 0, 0, 4, 4, 5,
    5, 3, 5, 6, 6, 7,
    4, 7, 7, 1, 6, 2
};

BoundingBoxRenderer::BoundingBoxRenderer()
    : Renderable(RenderBucket::RB_TRANSPARENT),
      m_mesh(new Mesh)
{
    m_vertices.resize(8);

    m_mesh->SetPrimitiveType(Mesh::PRIM_LINES);

    ShaderProperties properties = {};
    m_shader.reset(new Shader(properties, ShaderCode::aabb_debug_vs, ShaderCode::aabb_debug_fs));
}

BoundingBoxRenderer::~BoundingBoxRenderer()
{
    delete m_mesh;
}

void BoundingBoxRenderer::UpdateVertices()
{
    auto corners = m_aabb.GetCorners();

    for (int i = 0; i < corners.size(); i++) {
        m_vertices[i].SetPosition(corners[i]);
    }
}

void BoundingBoxRenderer::Render()
{
    UpdateVertices();

    // glLineWidth(2.0f);
    m_shader->Use();
    m_mesh->SetVertices(m_vertices, BoundingBoxRenderer::indices);
    m_mesh->Render();
    m_shader->End();
    // glLineWidth(1.0f);
}
} // namespace apex
