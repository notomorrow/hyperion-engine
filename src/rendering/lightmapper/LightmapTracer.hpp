#ifndef HYPERION_V2_LIGHTMAP_TRACER_HPP
#define HYPERION_V2_LIGHTMAP_TRACER_HPP

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/FixedArray.hpp>

#include <rendering/lightmapper/LightmapSparseVoxelOctree.hpp>

#include <rendering/Light.hpp>
#include <rendering/Mesh.hpp>

#include <scene/Scene.hpp>
#include <scene/NodeProxy.hpp>

#include <math/Ray.hpp>
#include <math/Vector3.hpp>
#include <math/Transform.hpp>

#include <util/NoiseFactory.hpp>

namespace hyperion::v2 {

struct LightmapTracerParams
{
    Handle<Light>   light;
    Handle<Scene>   scene;
};

struct LightmapMeshTraceData
{
    // // @TODO Use a more efficient data structure like svo for recording hits on a mesh
    // Array<Vec4f>    vertex_colors;

    HashMap<Vec3f, Pair<Vec4f, uint32>>  hits_map;

    LightmapOctree  octree;
    Transform       transform;
};

struct LightmapTraceData
{
    HashMap<ID<Mesh>, LightmapMeshTraceData>    elements;
};

struct LightmapRayHit
{
    ID<Entity>  entity_id;
    ID<Mesh>    mesh_id;
    uint        triangle_index = ~0u;
    RayHit      ray_hit;
    Ray         ray;

    bool operator<(const LightmapRayHit &other) const
        { return ray_hit < other.ray_hit; }

    bool operator==(const LightmapRayHit &other) const
    {
        return entity_id == other.entity_id
            && mesh_id == other.mesh_id
            && triangle_index == other.triangle_index
            && ray_hit == other.ray_hit
            && ray == other.ray;
    }

    bool operator!=(const LightmapRayHit &other) const
        { return !(*this == other); }
};

class LightmapTracer
{
public:
    static constexpr uint num_rays_per_light = 10000;
    static constexpr uint num_bounces = 3;//8;

    struct Result
    {

        enum Status
        {
            RESULT_OK,
            RESULT_ERR
        } status;

        String  message;

        Handle<Mesh>    debug_octree_mesh; // for debugging only -- to be removed
        
        Result()
            : status(RESULT_OK)
        { }

        Result(Status status)
            : status(status)
        { }

        // Temp constructor
        Result(Status status, Handle<Mesh> debug_octree_mesh)
            : status(status),
              debug_octree_mesh(std::move(debug_octree_mesh))
        { }

        Result(Status status, String message)
            : status(status),
              message(std::move(message))
        { }

        explicit operator bool() const
            { return status == RESULT_OK; }
    };

    LightmapTracer(const LightmapTracerParams &params);
    LightmapTracer(const LightmapTracer &other)                 = delete;
    LightmapTracer &operator=(const LightmapTracer &other)      = delete;
    LightmapTracer(LightmapTracer &&other) noexcept             = delete;
    LightmapTracer &operator=(LightmapTracer &&other) noexcept  = delete;
    ~LightmapTracer()                                           = default;

    Result Trace();

private:
    struct MeshDataCache
    {
        HashMap<ID<Mesh>, StreamedDataRef<StreamedMeshData>>    elements;
    };

    void PreloadMeshData();
    void CollectMeshes(NodeProxy node, Proc<void, const Handle<Mesh> &, const Transform &> &proc);

    void HandleRayHit(const LightmapRayHit &ray_hit, uint depth = 0);

    Optional<LightmapRayHit> TraceSingleRay(const Ray &ray);

    LightmapTracerParams        m_params;

    BasicNoiseGenerator<float>  m_noise_generator;

    MeshDataCache               m_mesh_data_cache;
    LightmapTraceData           m_trace_data;
};

} // namespace hyperion::v2

#endif