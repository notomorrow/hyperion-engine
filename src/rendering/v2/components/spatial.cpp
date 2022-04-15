#include "spatial.h"
#include "graphics.h"
#include "../engine.h"

namespace hyperion::v2 {

Spatial::Spatial(
    Ref<Mesh> &&mesh,
    const MeshInputAttributeSet &attributes,
    Ref<Material> &&material)
    : EngineComponentBase(),
      m_mesh(std::move(mesh)),
      m_attributes(attributes),
      m_material(std::move(material)),
      m_octree(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAabb();
        m_world_aabb = m_local_aabb * m_transform;
    }
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

        if (m_skeleton) {
            m_skeleton.Init();
        }

        if (m_octree == nullptr) {
            AddToOctree(engine);
        }

        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](Engine *engine) {
            RemoveFromPipelines();

            if (m_octree != nullptr) {
                RemoveFromOctree(engine);
            }
        }), engine);
    }));
}

void Spatial::Update(Engine *engine)
{
    if (m_skeleton != nullptr) {
        m_skeleton->UpdateShaderData(engine);
    }

    if (m_material != nullptr) {
        m_material->Update(engine);
    }

    if (!m_shader_data_state.IsDirty()) {
        if (m_octree != nullptr) {
            m_visibility_state = m_octree->GetVisibilityState();
        }

        return;
    }

    UpdateShaderData(engine);

    if (m_octree != nullptr) {
        UpdateOctree(engine);
    }
}

void Spatial::UpdateShaderData(Engine *engine) const
{
    engine->shader_globals->objects.Set(
        m_id.value - 1,
        {
            .model_matrix = m_transform.GetMatrix(),
            .has_skinning = m_skeleton != nullptr
        }
    );

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Spatial::UpdateOctree(Engine *engine)
{
    if (!m_octree->Update(engine, this)) {
        DebugLog(
            LogType::Warn,
            "Could not update Spatial #%lu in octree\n",
            m_id.value
        );
    }

    if (m_octree != nullptr) {
        m_visibility_state = m_octree->GetVisibilityState();
    }
}

void Spatial::SetMesh(Ref<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
    }

    m_mesh = std::move(mesh);

    if (m_mesh != nullptr && IsInit()) {
        m_mesh.Init();
    }
}

void Spatial::SetMaterial(Ref<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    m_material = std::move(material);

    if (m_material != nullptr && IsInit()) {
        m_material.Init();
    }
}

void Spatial::SetSkeleton(Ref<Skeleton> &&skeleton)
{
    if (m_skeleton == skeleton) {
        return;
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton != nullptr && IsInit()) {
        m_skeleton.Init();
    }
}

void Spatial::SetTransform(const Transform &transform)
{
    m_transform = transform;
    m_shader_data_state |= ShaderDataState::DIRTY;

    m_world_aabb = m_local_aabb * transform;
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

void Spatial::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);

    m_octree = octree;
}

void Spatial::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(m_octree != nullptr);

    m_octree = nullptr;
}

void Spatial::AddToOctree(Engine *engine)
{
    AssertThrow(m_octree == nullptr);

    if (!engine->GetOctree().Insert(engine, this)) {
        DebugLog(LogType::Warn, "Spatial #%lu could not be added to octree\n", m_id.value);
    }
}

void Spatial::RemoveFromOctree(Engine *engine)
{
    AssertThrow(m_octree != nullptr);

    m_octree->OnSpatialRemoved(engine, this);
}

} // namespace hyperion::v2