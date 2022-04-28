#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include "base.h"
#include "material.h"
#include "mesh.h"
#include "skeleton.h"
#include "octree.h"

#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>
#include <math/bounding_box.h>

//#include <rendering/mesh.h>

#include <vector>

#include "octree.h"

namespace hyperion::v2 {

class Octree;

using renderer::MeshInputAttributeSet;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;

class GraphicsPipeline;

class Spatial : public EngineComponentBase<STUB_CLASS(Spatial)> {
    friend class Engine;
    friend class Octree;
    friend class GraphicsPipeline;

public:
    Spatial(
        Ref<Mesh> &&mesh,
        const MeshInputAttributeSet &attributes,
        Ref<Material> &&material
    );

    Spatial(const Spatial &other) = delete;
    Spatial &operator=(const Spatial &other) = delete;
    ~Spatial();

    ShaderDataState GetShaderDataState() const { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }
    
    Octree::VisibilityState &GetVisibilityState() { return m_visibility_state; }
    const Octree::VisibilityState &GetVisibilityState() const { return m_visibility_state; }
    
    Mesh *GetMesh() const { return m_mesh.ptr; }
    void SetMesh(Ref<Mesh> &&mesh);

    Octree *GetOctree() const { return m_octree; }

    Material *GetMaterial() const { return m_material.ptr; }
    void SetMaterial(Ref<Material> &&material);

    Skeleton *GetSkeleton() const { return m_skeleton.ptr; }
    void SetSkeleton(Ref<Skeleton> &&skeleton);
    
    const MeshInputAttributeSet &GetVertexAttributes() const { return m_attributes; }

    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform);

    const BoundingBox &GetLocalAabb() const { return m_local_aabb; }
    const BoundingBox &GetWorldAabb() const { return m_world_aabb; }

    bool HasAccelerationGeometry() const { return m_acceleration_geometry != nullptr; }
    AccelerationGeometry *GetAccelerationGeometry() const { return m_acceleration_geometry.get(); }
    AccelerationGeometry *CreateAccelerationGeometry(Engine *engine);
    void DestroyAccelerationGeometry(Engine *engine);

    void Init(Engine *engine);
    void Update(Engine *engine);

private:
    void UpdateShaderData(Engine *engine) const;
    void UpdateOctree(Engine *engine);
    
    void OnAddedToPipeline(GraphicsPipeline *pipeline);
    void OnRemovedFromPipeline(GraphicsPipeline *pipeline);
    void RemoveFromPipelines();
    void RemoveFromPipeline(GraphicsPipeline *pipeline);
    
    void OnAddedToOctree(Octree *octree);
    void OnRemovedFromOctree(Octree *octree);
    void AddToOctree(Engine *engine);
    void RemoveFromOctree(Engine *engine);

    Ref<Mesh> m_mesh;
    MeshInputAttributeSet m_attributes;
    Transform m_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;
    Ref<Material> m_material;
    Ref<Skeleton> m_skeleton;
    
    std::unique_ptr<AccelerationGeometry> m_acceleration_geometry;

    Octree *m_octree;
    Octree::VisibilityState m_visibility_state;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    std::vector<GraphicsPipeline *> m_pipelines;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif