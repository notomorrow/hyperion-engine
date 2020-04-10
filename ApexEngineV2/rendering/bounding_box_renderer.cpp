#include "bounding_box_renderer.h"
#include "shaders/shader_code.h"

#include <vector>

#include <stddef.h>

namespace apex {
BoundingBoxRenderer::BoundingBoxRenderer(const BoundingBox *bounding_box)
    : Renderable(RenderBucket::RB_TRANSPARENT),
      m_bounding_box(bounding_box),
      m_mesh(new Mesh)
{
    m_mesh->SetPrimitiveType(Mesh::PRIM_LINES);

    ShaderProperties properties = {};
    m_shader.reset(new Shader(properties, ShaderCode::aabb_debug_vs, ShaderCode::aabb_debug_fs));
}

BoundingBoxRenderer::~BoundingBoxRenderer()
{
    delete m_mesh;
}

void BoundingBoxRenderer::Render()
{
    const Vector3 &min = m_bounding_box->GetMin();
    const Vector3 &max = m_bounding_box->GetMax();

    auto corners = m_bounding_box->GetCorners();

    std::vector<Vertex> vertices(corners.size());
    for (int i = 0; i < corners.size(); i++) {
        vertices[i].SetPosition(corners[i]);
    }

    std::vector<size_t> indices = {
        0, 1, 1, 2, 2, 3,
        3, 0, 0, 4, 4, 5,
        5, 3, 5, 6, 6, 7,
        4, 7, 7, 1, 6, 2
    };

    glLineWidth(2.0f);
    m_mesh->SetVertices(vertices, indices);
    m_mesh->Render();
    glLineWidth(1.0f);
}
} // namespace apex
