#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include <rendering/base.h>
#include <rendering/material.h>
#include <rendering/mesh.h>
#include <rendering/shader.h>
#include <rendering/render_bucket.h>
#include <animation/skeleton.h>
#include <core/scheduler.h>

#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>
#include <math/bounding_box.h>

#include <vector>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;

class GraphicsPipeline;
class Octree;

class Spatial : public EngineComponentBase<STUB_CLASS(Spatial)> {
    friend class Engine;
    friend class Octree;
    friend class GraphicsPipeline;

public:
    Spatial(
        Ref<Mesh> &&mesh,
        Ref<Shader> &&shader,
        const VertexAttributeSet &attributes,
        Ref<Material> &&material,
        Bucket bucket = Bucket::BUCKET_OPAQUE
    );

    Spatial(const Spatial &other) = delete;
    Spatial &operator=(const Spatial &other) = delete;
    ~Spatial();

    Octree *GetOctree() const { return m_octree; }

    ShaderDataState GetShaderDataState() const     { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }
    
    Mesh *GetMesh() const { return m_mesh.ptr; }
    void SetMesh(Ref<Mesh> &&mesh);

    Ref<Skeleton> &GetSkeleton()             { return m_skeleton; }
    const Ref<Skeleton> &GetSkeleton() const { return m_skeleton; }
    void SetSkeleton(Ref<Skeleton> &&skeleton);

    Shader *GetShader() const     { return m_shader.ptr; }
    void SetShader(Ref<Shader> &&shader);

    Material *GetMaterial() const { return m_material.ptr; }
    void SetMaterial(Ref<Material> &&material);

    Bucket GetBucket() const      { return m_bucket; }
    void SetBucket(Bucket bucket);
    
    const VertexAttributeSet &GetVertexAttributes() const { return m_attributes; }

    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform);

    const BoundingBox &GetLocalAabb() const { return m_local_aabb; }
    const BoundingBox &GetWorldAabb() const { return m_world_aabb; }

    void Init(Engine *engine);
    void Update(Engine *engine);

private:
    void EnqueueRenderUpdates(Engine *engine);
    void UpdateOctree(Engine *engine);
    
    void OnAddedToPipeline(GraphicsPipeline *pipeline);
    void OnRemovedFromPipeline(GraphicsPipeline *pipeline);
    
    void AddToDefaultPipeline(Engine *engine);
    void AddToPipeline(Engine *engine, GraphicsPipeline *pipeline);
    void RemoveFromPipelines(Engine *engine);
    void RemoveFromPipeline(Engine *engine, GraphicsPipeline *pipeline);
    
    void OnAddedToOctree(Octree *octree);
    void OnRemovedFromOctree(Octree *octree);
    void OnMovedToOctant(Octree *octree);

    void AddToOctree(Engine *engine);
    void RemoveFromOctree(Engine *engine);

    Ref<Mesh> m_mesh;
    Ref<Shader> m_shader;
    VertexAttributeSet m_attributes;
    Transform m_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;
    Ref<Material> m_material;
    Ref<Skeleton> m_skeleton;
    Bucket m_bucket;

    std::atomic<Octree *> m_octree{nullptr};

    struct {
        GraphicsPipeline *pipeline = nullptr;
        bool changed = false;
    } m_primary_pipeline;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    std::vector<GraphicsPipeline *> m_pipelines;

    mutable ShaderDataState m_shader_data_state;
    ScheduledFunctionId     m_render_update_id;
};

} // namespace hyperion::v2

#endif