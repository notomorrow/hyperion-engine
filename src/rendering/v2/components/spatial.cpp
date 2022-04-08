#include "spatial.h"
#include "graphics.h"
#include "../engine.h"

namespace hyperion::v2 {

Spatial::Spatial(
    EngineCallbacks &callbacks,
    Mesh *mesh,
    const MeshInputAttributeSet &attributes,
    const Transform &transform,
    const BoundingBox &local_aabb,
    Material::ID material_id)
    : EngineComponent(callbacks),
      m_mesh(mesh),
      m_attributes(attributes),
      m_transform(transform),
      m_local_aabb(local_aabb),
      m_world_aabb(local_aabb * transform),
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
    engine->resources.meshes.Acquire(engine, m_mesh);
}

void Spatial::Destroy(Engine *engine)
{
    engine->resources.meshes.Release(engine, m_mesh);

    RemoveFromPipelines();
}

} // namespace hyperion::v2