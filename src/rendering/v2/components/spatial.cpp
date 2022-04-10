#include "spatial.h"
#include "graphics.h"
#include "../engine.h"

namespace hyperion::v2 {

Spatial::Spatial(
    Ref<Mesh> &&mesh,
    const MeshInputAttributeSet &attributes,
    const Transform &transform,
    const BoundingBox &local_aabb,
    Ref<Material> &&material)
    : EngineComponentBase(),
      m_mesh(std::move(mesh)),
      m_attributes(attributes),
      m_transform(transform),
      m_local_aabb(local_aabb),
      m_world_aabb(local_aabb * transform),
      m_material(std::move(material))
{
}

Spatial::~Spatial()
{
    Teardown();
}

void Spatial::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SPATIALS, [this](Engine *engine) {
        if (m_mesh) {
            m_mesh.Init();
        }

        if (m_material) {
            m_material.Init();
        }

        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](Engine *engine) {
            RemoveFromPipelines();
        }), engine);
    }));
}

void Spatial::SetMaterial(Ref<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    m_material = std::move(material);

    if (IsInit() && m_material != nullptr) {
        m_material.Init();
    }
}

void Spatial::UpdateShaderData(Engine *engine) const
{
    engine->shader_globals->objects.Set(
        m_id.value - 1,
        {.model_matrix = m_transform.GetMatrix()}
    );
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

} // namespace hyperion::v2