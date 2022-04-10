#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include "base.h"
#include "material.h"
#include "mesh.h"

#include <rendering/backend/renderer_structs.h>

#include <math/matrix4.h>
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
    
    Mesh *GetMesh() const { return m_mesh.ptr; }
    Material *GetMaterial() const { return m_material.ptr; }

    const MeshInputAttributeSet &GetVertexAttributes() const { return m_attributes; }

    const Transform &GetTransform() const { return m_transform; }
    inline void SetTransform(const Transform &transform)
    {
        m_transform = transform;

        m_world_aabb = m_local_aabb * transform;
    }

    const BoundingBox &GetLocalAabb() const { return m_local_aabb; }
    const BoundingBox &GetWorldAabb() const { return m_world_aabb; }

    void Init(Engine *engine);

private:
    void UpdateShaderData(Engine *engine) const;

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

    Octree *m_octree;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    std::vector<GraphicsPipeline *> m_pipelines;
};

} // namespace hyperion::v2

#endif