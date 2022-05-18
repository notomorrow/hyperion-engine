#include "spatial.h"
#include <rendering/graphics.h>
#include <engine.h>

namespace hyperion::v2 {

Spatial::Spatial(
    Ref<Mesh> &&mesh,
    Ref<Shader> &&shader,
    const VertexAttributeSet &attributes,
    Ref<Material> &&material,
    Bucket bucket)
    : EngineComponentBase(),
      m_mesh(std::move(mesh)),
      m_shader(std::move(shader)),
      m_attributes(attributes),
      m_material(std::move(material)),
      m_bucket(bucket),
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
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SPATIALS, [this](Engine *engine) {
        if (m_material) {
            m_material.Init();
        }

        if (m_skeleton) {
            m_skeleton.Init();
        }

        if (m_mesh) {
            m_mesh.Init();

            if (m_primary_pipeline.pipeline == nullptr) {
                AddToDefaultPipeline(engine);
            }
        }

        if (m_octree == nullptr) {
            AddToOctree(engine);
        }

        SetReady(true);

        //EnqueueRenderUpdates(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](Engine *engine) {
            RemoveFromPipelines(engine);

            if (m_octree != nullptr) {
                RemoveFromOctree(engine);
            }
            
            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Spatial::Update(Engine *engine)
{
    AssertReady();

    if (m_skeleton != nullptr) {
        m_skeleton->EnqueueRenderUpdates(engine);
    }

    if (m_material != nullptr) {
        m_material->Update(engine);
    }

    if (m_primary_pipeline.changed) {
        if (m_primary_pipeline.pipeline != nullptr) {
            RemoveFromPipeline(engine, m_primary_pipeline.pipeline);
        }

        AddToDefaultPipeline(engine);
    }

    if (m_shader_data_state.IsDirty()) {
        if (Octree *octree = m_octree.load()) {
            UpdateOctree(engine, octree);
        }

        EnqueueRenderUpdates(engine);
    }
}

void Spatial::EnqueueRenderUpdates(Engine *engine)
{
    AssertReady();

    m_render_update_id = engine->render_scheduler.EnqueueReplace(m_render_update_id, [this, engine, transform = m_transform](...) {
        engine->shader_globals->objects.Set(
            m_id.value - 1,
            {
                .model_matrix   = transform.GetMatrix(),
                .has_skinning   = m_skeleton != nullptr,
                .material_index = m_material != nullptr
                    ? m_material->GetId().value - 1
                    : 0,
                .local_aabb_max         = Vector4(m_local_aabb.max, 1.0f),
                .local_aabb_min         = Vector4(m_local_aabb.min, 1.0f),
                .world_aabb_max         = Vector4(m_world_aabb.max, 1.0f),
                .world_aabb_min         = Vector4(m_world_aabb.min, 1.0f)
            }
        );

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Spatial::UpdateOctree(Engine *engine, Octree *octree)
{
    if (!octree->Update(engine, this)) {
        DebugLog(
            LogType::Warn,
            "Could not update Spatial #%lu in octree\n",
            m_id.value
        );
    }
}

void Spatial::SetMesh(Ref<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
    }

    m_mesh = std::move(mesh);

    if (m_mesh != nullptr && IsReady()) {
        m_mesh.Init();
    }
}

void Spatial::SetSkeleton(Ref<Skeleton> &&skeleton)
{
    if (m_skeleton == skeleton) {
        return;
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton != nullptr && IsReady()) {
        m_skeleton.Init();
    }
}

void Spatial::SetShader(Ref<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);
    m_primary_pipeline.changed = true;

    if (m_shader != nullptr && IsReady()) {
        m_shader.Init();
    }
}

void Spatial::SetMaterial(Ref<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    m_material = std::move(material);
    m_primary_pipeline.changed = true;

    if (m_material != nullptr && IsReady()) {
        m_material.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::SetBucket(Bucket bucket)
{
    if (m_bucket == bucket) {
        return;
    }

    m_bucket = bucket;
    m_primary_pipeline.changed = true;
}

void Spatial::SetTransform(const Transform &transform)
{
    if (m_transform == transform) {
        return;
    }

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
    if (pipeline == m_primary_pipeline.pipeline) {
        m_primary_pipeline = {.pipeline = nullptr, .changed = true};
    }

    const auto it = std::find(m_pipelines.begin(), m_pipelines.end(), pipeline);

    if (it != m_pipelines.end()) {
        m_pipelines.erase(it);
    }
}

void Spatial::AddToDefaultPipeline(Engine *engine)
{
    if (const auto pipeline = engine->FindOrCreateGraphicsPipeline(m_shader.IncRef(), m_mesh->GetVertexAttributes(), m_bucket)) {
        AddToPipeline(engine, pipeline.ptr);

        if (!m_pipelines.empty()) {
            m_primary_pipeline = {.pipeline = m_pipelines.front(), .changed = false};
        }
    } else {
        DebugLog(
            LogType::Error,
            "Could not find optimal graphics pipeline for Spatial #%lu! Bucket index: %d\n",
            m_id.value,
            int(m_bucket)
        );
    }
}

void Spatial::AddToPipeline(Engine *engine, GraphicsPipeline *pipeline)
{
    pipeline->AddSpatial(engine->resources.spatials.IncRef(this));
}

void Spatial::RemoveFromPipelines(Engine *)
{
    for (auto *pipeline : m_pipelines) {
        pipeline->OnSpatialRemoved(this);
    }

    m_pipelines.clear();
    
    m_primary_pipeline = {.pipeline = nullptr, .changed = true};
}

void Spatial::RemoveFromPipeline(Engine *, GraphicsPipeline *pipeline)
{
    if (pipeline == m_primary_pipeline.pipeline) {
        m_primary_pipeline = {.pipeline = nullptr, .changed = true};
    }

    pipeline->OnSpatialRemoved(this);

    OnRemovedFromPipeline(pipeline);
}

void Spatial::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu added to octree\n", m_id.value);
#endif

    m_octree = octree;
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(octree == m_octree);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu removed from octree\n", m_id.value);
#endif

    m_octree = nullptr;
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnMovedToOctant(Octree *octree)
{
    AssertThrow(m_octree != nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu moved to new octant\n", m_id.value);
#endif

    m_octree = octree;
    m_shader_data_state |= ShaderDataState::DIRTY;
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
    auto *octree = m_octree.load();
    AssertThrow(octree != nullptr);

    octree->OnSpatialRemoved(engine, this);
}

} // namespace hyperion::v2