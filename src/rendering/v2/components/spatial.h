#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include "base.h"
#include "material.h"
#include "mesh.h"
#include "skeleton.h"

#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>
#include <math/bounding_box.h>

//#include <rendering/mesh.h>

#include <vector>

namespace hyperion::v2 {

class Octree;

using renderer::MeshInputAttributeSet;

class GraphicsPipeline;

class Spatial : public EngineComponentBase<STUB_CLASS(Spatial)> {
    friend class Engine;
    friend class GraphicsPipeline;

public:
    Spatial(
        Ref<Mesh> &&mesh,
        const MeshInputAttributeSet &attributes,
        const Transform &transform,
        const BoundingBox &local_aabb,
        Ref<Material> &&material);
    Spatial(const Spatial &other) = delete;
    Spatial &operator=(const Spatial &other) = delete;
    ~Spatial();

    ShaderDataState GetShaderDataState() const { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }
    
    Mesh *GetMesh() const { return m_mesh.ptr; }

    Material *GetMaterial() const { return m_material.ptr; }
    void SetMaterial(Ref<Material> &&material);

    Skeleton *GetSkeleton() const { return m_skeleton.ptr; }
    void SetSkeleton(Ref<Skeleton> &&skeleton);

    const MeshInputAttributeSet &GetVertexAttributes() const { return m_attributes; }

    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform);

    const BoundingBox &GetLocalAabb() const { return m_local_aabb; }
    const BoundingBox &GetWorldAabb() const { return m_world_aabb; }

    void Init(Engine *engine);
    void UpdateShaderData(Engine *engine) const;

private:

    void OnAddedToPipeline(GraphicsPipeline *pipeline);
    void OnRemovedFromPipeline(GraphicsPipeline *pipeline);
    void RemoveFromPipelines();
    void RemoveFromPipeline(GraphicsPipeline *pipeline);

    Ref<Mesh> m_mesh;
    MeshInputAttributeSet m_attributes;
    Transform m_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;
    Ref<Material> m_material;
    Ref<Skeleton> m_skeleton;

    Octree *m_octree;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    std::vector<GraphicsPipeline *> m_pipelines;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif