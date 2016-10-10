#include "bounding_box_renderer.h"
#include "shaders/shader_code.h"

#include <vector>

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

    std::vector<uint32_t> indices = {
        0, 1, 1, 2, 2, 3,
        3, 0, 0, 4, 4, 5,
        5, 3, 5, 6, 6, 7,
        4, 7, 7, 1, 6, 2
    };

    /*std::vector<Vector3> new_vecs;
    std::vector<int> new_idc;
    int idc_counter = 0;
    for (int i = 0; i < vecs.size(); i++) {
        Vector3 vec = vecs[i];
        int dup_idx = -1;
        // find duplicate
        for (int j = 0; j < new_vecs.size(); j++) {
            if (vec == new_vecs[j]) {
                // duplicate found
                dup_idx = j;
                break;
            }
        }

        if (dup_idx == -1) {
            new_idc.push_back(new_vecs.size());
            new_vecs.push_back(vec);
        } else {
            new_idc.push_back(dup_idx);
        }
    }

    std::cout << "new_vecs: {\n";
    for (auto i : new_vecs) {
        std::cout << i << ", ";
    }
    std::cout << "}\n\n";
    std::cout << "new_idc: {\n";
    for (auto i : new_idc) {
        std::cout << i << ", ";
    }
    std::cout << "}\n\n";*/

    glLineWidth(2.0f);
    m_mesh->SetVertices(vertices, indices);
    m_mesh->Render();
    glLineWidth(1.0f);
}
} // namespace apex