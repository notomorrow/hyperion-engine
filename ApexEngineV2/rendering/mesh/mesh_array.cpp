#include "mesh_array.h"

#include "../../util/mesh_factory.h"

namespace apex {
MeshArray::MeshArray()
    : Renderable()
{
}

void MeshArray::Render()
{
    for (auto &submesh : m_submeshes) {
        submesh.mesh->Render();
    }
}

void MeshArray::UpdateSubmeshes()
{
    if (m_submeshes.empty()) {
        return;
    }

    if (m_submeshes[0].mesh != nullptr) {
        SetRenderBucket(m_submeshes[0].mesh->GetRenderBucket());
    }

    ApplyTransforms();
}

void MeshArray::ApplyTransforms()
{
    for (auto &submesh : m_submeshes) {
        submesh.mesh = MeshFactory::TransformMesh(
            submesh.mesh,
            submesh.transform
        );

        // reset transform
        submesh.transform = Transform();
    }
}

void MeshArray::Optimize()
{
    if (m_submeshes.size() <= 1) {
        return;
    }

    for (size_t i = 1; i < m_submeshes.size(); i++) {
        m_submeshes[0].mesh = MeshFactory::MergeMeshes(
            m_submeshes[0].mesh,
            m_submeshes[i].mesh,
            m_submeshes[0].transform,
            m_submeshes[i].transform
        );
    }

    m_submeshes.resize(1);
}
} // namespace apex
