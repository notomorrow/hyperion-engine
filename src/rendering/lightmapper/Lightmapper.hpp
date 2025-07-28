/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Queue.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <core/config/Config.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

namespace hyperion {

static constexpr int maxBouncesCpu = 4;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct LightmapHitsBuffer;
class LightmapThreadPool;
class ILightmapAccelerationStructure;
class LightmapJob;
class LightmapVolume;

class View;
struct RenderSetup;

enum class LightmapTraceMode : int
{
    GPU_PATH_TRACING = 0,
    CPU_PATH_TRACING,

    MAX
};

enum class LightmapShadingType
{
    IRRADIANCE,
    RADIANCE,
    MAX
};

HYP_STRUCT(ConfigName = "app", JsonPath = "lightmapper")
struct LightmapperConfig : public ConfigBase<LightmapperConfig>
{
    HYP_FIELD(JsonPath = "trace_mode")
    LightmapTraceMode traceMode = LightmapTraceMode::GPU_PATH_TRACING;

    HYP_FIELD(JsonPath = "radiance")
    bool radiance = true;

    HYP_FIELD(JsonPath = "irradiance")
    bool irradiance = true;

    HYP_FIELD(JsonPath = "num_samples")
    uint32 numSamples = 16;

    HYP_FIELD(JsonPath = "max_rays_per_frame")
    uint32 maxRaysPerFrame = 512 * 512;

    HYP_FIELD(JsonPath = "ideal_triangles_per_job")
    uint32 idealTrianglesPerJob = 8192;

    virtual ~LightmapperConfig() override = default;

    HYP_API void PostLoadCallback();

    bool Validate()
    {
        bool valid = true;

        if (uint32(traceMode) >= uint32(LightmapTraceMode::MAX))
        {
            AddError(HYP_MAKE_ERROR(Error, "Invalid trace mode"));

            valid = false;
        }

        if (!radiance && !irradiance)
        {
            AddError(HYP_MAKE_ERROR(Error, "At least one of radiance or irradiance must be enabled"));

            valid = false;
        }

        if (numSamples == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Number of samples must be greater than zero"));

            valid = false;
        }

        if (maxRaysPerFrame == 0 || maxRaysPerFrame > 1024 * 1024)
        {
            AddError(HYP_MAKE_ERROR(Error, "Max rays per frame must be greater than zero and less than or equal to 1024*1024"));

            valid = false;
        }

        if (idealTrianglesPerJob == 0 || idealTrianglesPerJob > 100000)
        {
            AddError(HYP_MAKE_ERROR(Error, "Ideal triangles per job must be greater than zero and less than or equal to 100000"));

            valid = false;
        }

        return valid;
    }
};

struct LightmapHit
{
    Vec4f color;
};

static_assert(sizeof(LightmapHit) == 16);

class LightmapTopLevelAccelerationStructure;

struct LightmapRayHitPayload
{
    Vec4f throughput;
    Vec4f emissive;
    Vec4f radiance;
    Vec3f normal;
    float distance = -1.0f;
    Vec3f barycentricCoords;
    ObjId<Mesh> meshId;
    uint32 triangleIndex = ~0u;
};

class ILightmapRenderer
{
public:
    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxRaysPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual void Create() = 0;
    virtual void UpdateRays(Span<const LightmapRay> rays) = 0;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) = 0;
};

struct LightmapJobParams
{
    LightmapperConfig* config;

    Handle<Scene> scene;
    Handle<LightmapVolume> volume;

    Span<LightmapSubElement> subElementsView;
    HashMap<Handle<Entity>, LightmapSubElement*>* subElementsByEntity;

    LightmapTopLevelAccelerationStructure* accelerationStructure;

    Array<ILightmapRenderer*> renderers;
};

class HYP_API LightmapJob
{
public:
    friend struct RenderCommand_LightmapRender;

    LightmapJob(LightmapJobParams&& params);
    LightmapJob(const LightmapJob& other) = delete;
    LightmapJob& operator=(const LightmapJob& other) = delete;
    LightmapJob(LightmapJob&& other) noexcept = delete;
    LightmapJob& operator=(LightmapJob&& other) noexcept = delete;
    ~LightmapJob();

    HYP_FORCE_INLINE const LightmapJobParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_FORCE_INLINE LightmapUVBuilder& GetUVBuilder()
    {
        return m_uvBuilder;
    }

    HYP_FORCE_INLINE const LightmapUVBuilder& GetUVBuilder() const
    {
        return m_uvBuilder;
    }

    HYP_FORCE_INLINE LightmapUVMap& GetUVMap()
    {
        return *m_uvMap;
    }

    HYP_FORCE_INLINE const LightmapUVMap& GetUVMap() const
    {
        return *m_uvMap;
    }

    HYP_FORCE_INLINE uint32 GetElementIndex() const
    {
        return m_elementIndex;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_params.scene.Get();
    }

    HYP_FORCE_INLINE Span<LightmapSubElement> GetSubElements() const
    {
        return m_params.subElementsView;
    }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
    {
        return m_texelIndex;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetTexelIndices() const
    {
        return m_texelIndices;
    }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay>& outRays) const
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        outRays = m_previousFrameRays;
    }

    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay>& rays)
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        m_previousFrameRays = rays;
    }

    HYP_FORCE_INLINE const Result& GetResult() const
    {
        return m_result;
    }

    void Start();
    void Process();

    void GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays);

    void AddTask(TaskBatch* taskBatch);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType);

    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_runningSemaphore.IsInSignalState();
    }

private:
    struct CachedResource
    {
        Handle<AssetObject> assetObject;
        ResourceHandle resourceHandle;

        CachedResource() = default;

        CachedResource(const Handle<AssetObject>& assetObject, const ResourceHandle& resourceHandle)
            : assetObject(assetObject),
              resourceHandle(resourceHandle)
        {
        }

        CachedResource(const CachedResource& other) = delete;
        CachedResource& operator=(const CachedResource& other) = delete;

        CachedResource(CachedResource&& other) noexcept
            : assetObject(std::move(other.assetObject)),
              resourceHandle(std::move(other.resourceHandle))
        {
            other.resourceHandle.Reset();
        }

        CachedResource& operator=(CachedResource&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            resourceHandle.Reset();

            assetObject = std::move(other.assetObject);
            resourceHandle = std::move(other.resourceHandle);

            return *this;
        }

        ~CachedResource()
        {
            // destruct the ResourceHandle before assetobject is destructed,
            // so it destructing the AssetObject doesn't try to wait for the resource's ref count to reach zero
            resourceHandle.Reset();
        }
    };

    using ResourceCache = HashSet<CachedResource, &CachedResource::assetObject, HashTable_DynamicNodeAllocator<CachedResource>>;

    HYP_FORCE_INLINE bool HasRemainingTexels() const
    {
        return m_texelIndex < m_texelIndices.Size() * m_params.config->numSamples;
    }

    /*! \brief Get the next texel index to process, advancing the teexl counter
     *  \return The texel index
     */
    HYP_FORCE_INLINE uint32 NextTexel()
    {
        const uint32 currentTexelIndex = m_texelIndices[m_texelIndex % m_texelIndices.Size()];
        m_texelIndex++;

        return currentTexelIndex;
    }

    void Stop();
    void Stop(const Error& error);

    LightmapJobParams m_params;

    UUID m_uuid;

    Handle<View> m_view;

    Array<uint32> m_texelIndices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay> m_previousFrameRays;
    mutable Mutex m_previousFrameRaysMutex;

    LightmapUVBuilder m_uvBuilder;
    Optional<LightmapUVMap> m_uvMap;
    Task<TResult<LightmapUVMap>> m_buildUvMapTask;

    uint32 m_elementIndex;

    Array<TaskBatch*> m_currentTasks;
    mutable Mutex m_currentTasksMutex;

    Semaphore<int32> m_runningSemaphore;
    uint32 m_texelIndex;

    double m_lastLoggedPercentage;

    AtomicVar<uint32> m_numConcurrentRenderingTasks;

    // cache to keep asset data in memory while we're using it
    ResourceCache m_resourceCache;

    Result m_result;
};

class HYP_API Lightmapper
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);
    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;
    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;
    ~Lightmapper();

    bool IsComplete() const;

    void PerformLightmapping();
    void Update(float delta);

    Delegate<void> OnComplete;

private:
    LightmapJobParams CreateLightmapJobParams(
        SizeType startIndex,
        SizeType endIndex,
        LightmapTopLevelAccelerationStructure* accelerationStructure);

    void AddJob(UniquePtr<LightmapJob>&& job)
    {
        Mutex::Guard guard(m_queueMutex);

        m_queue.Push(std::move(job));

        m_numJobs.Increment(1, MemoryOrder::RELEASE);
    }

    void HandleCompletedJob(LightmapJob* job);

    LightmapperConfig m_config;

    Handle<Scene> m_scene;
    BoundingBox m_aabb;

    Handle<LightmapVolume> m_volume;

    Array<UniquePtr<ILightmapRenderer>> m_lightmapRenderers;

    UniquePtr<LightmapTopLevelAccelerationStructure> m_accelerationStructure;

    Queue<UniquePtr<LightmapJob>> m_queue;
    Mutex m_queueMutex;
    AtomicVar<uint32> m_numJobs;

    Array<LightmapSubElement> m_subElements;
    HashMap<Handle<Entity>, LightmapSubElement*> m_subElementsByEntity;
};

} // namespace hyperion
