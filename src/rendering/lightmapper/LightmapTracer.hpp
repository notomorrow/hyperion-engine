#ifndef HYPERION_V2_LIGHTMAP_TRACER_HPP
#define HYPERION_V2_LIGHTMAP_TRACER_HPP

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/FixedArray.hpp>

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

struct LightmapHitData
{
    Vec3f       position;
    Vec4f       throughput;
    float       emissive;
    ID<Mesh>    mesh_id;
    uint32      triangle_index;
};

struct LightmapHitPath
{
    Array<LightmapHitData>  hits;

    void AddHit(LightmapHitData hit)
        { hits.PushBack(std::move(hit)); }

    bool Missed() const
        { return hits.Empty(); }
};

struct LightmapMeshTraceData
{
    // Don't use octree for now - having issues with it
    HashMap<Vec3f, LightmapHitData> hits_map;
};

struct LightmapTraceData
{
    // @TODO: Use more suitable data structure for this
    HashMap<Vec3f, LightmapHitData> hits_map;

    void IntegrateHit(const LightmapHitData &hit)
    {
        auto it = hits_map.Find(hit.position);
        // @TODO Use proper integration
        if (it != hits_map.End()) {
            it->second.throughput *= hit.throughput;
            it->second.emissive += hit.emissive;
        } else {
            hits_map.Insert(hit.position, hit);
        }
    }
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
    static constexpr uint num_rays_per_light = 40000;
    static constexpr uint num_bounces = 20;

    struct Result
    {

        enum Status
        {
            RESULT_OK,
            RESULT_ERR
        } status;

        String  message;
        
        Result()
            : status(RESULT_OK)
        { }

        Result(Status status)
            : status(status)
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

    void HandleRayHit(const LightmapRayHit &ray_hit, LightmapHitPath &path, uint depth = 0);

    Optional<LightmapRayHit> TraceSingleRay(const Ray &ray);

    LightmapTracerParams        m_params;

    BasicNoiseGenerator<float>  m_noise_generator;

    MeshDataCache               m_mesh_data_cache;
};

} // namespace hyperion::v2

#endif