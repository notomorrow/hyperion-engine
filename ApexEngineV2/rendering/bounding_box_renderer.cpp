#include "bounding_box_renderer.h"
#include "shaders/shader_code.h"

#include <vector>

namespace apex {

BoundingBoxRenderer::BoundingBoxRenderer(BoundingBox *bounding_box)
    : Renderable(RenderBucket::RB_TRANSPARENT),
      m_bounding_box(bounding_box),
      m_mesh(new Mesh)
{
    std::vector<Vertex> vertices(8); // 8 corners
    m_mesh->SetVertices(vertices);
    m_mesh->SetPrimitiveType(Mesh::PRIM_LINES);

    ShaderProperties properties = {};
    m_shader.reset(new Shader(properties, ShaderCode::basic_vs, ShaderCode::basic_fs));
}

BoundingBoxRenderer::~BoundingBoxRenderer()
{
    delete m_mesh;
}

void BoundingBoxRenderer::Render()
{
    auto corners = m_bounding_box->GetCorners();
    auto vertices = m_mesh->GetVertices();
    for (int i = 0; i < 8; i++) {
        vertices[i].SetPosition(corners[i]);
    }
    m_mesh->SetVertices(vertices);
    m_mesh->Render();
}

} // namespace apex