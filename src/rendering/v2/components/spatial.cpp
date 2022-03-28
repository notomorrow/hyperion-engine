#include "spatial.h"
#include "graphics.h"

namespace hyperion::v2 {

Spatial::Spatial(const std::shared_ptr<Mesh> &mesh,
    const MeshInputAttributeSet &attributes,
    const Transform &transform,
    Material::ID material_id)
    : m_mesh(mesh),
      m_attributes(attributes),
      m_transform(transform),
      m_material_id(material_id)
{
}

Spatial::~Spatial()
{
    RemoveFromPipelines();
}

void Spatial::OnAddedToPipeline(GraphicsPipeline *pipeline)
{
    m_pipelines.push_back(pipeline);
}

void Spatial::OnRemovedFromPipeline(GraphicsPipeline *pipeline)
{
    const auto it = std::find(m_pipelines.begin(), m_pipelines.end(), pipeline);

    if (it != m_pipelines.end()) {
        m_pipelines.erase(it);
    }
}

void Spatial::RemoveFromPipelines()
{
    for (auto *pipeline : m_pipelines) {
        pipeline->OnSpatialRemoved(this);
    }

    m_pipelines.clear();
}

void Spatial::RemoveFromPipeline(GraphicsPipeline *pipeline)
{
    pipeline->OnSpatialRemoved(this);

    OnRemovedFromPipeline(pipeline);
}

void Spatial::Create(Engine *engine)
{
    /* TODO */
}

void Spatial::Destroy(Engine *engine)
{
    RemoveFromPipelines();
}

} // namespace hyperion::v2