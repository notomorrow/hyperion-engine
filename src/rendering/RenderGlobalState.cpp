/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/rt/DDGI.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderBackend.hpp>

#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Material.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClass.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/MemoryPool.hpp>
#include <core/memory/pool/MemoryPool2.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <EngineGlobals.hpp>

#include <semaphore>

namespace hyperion {

static constexpr uint32 g_numFrames = g_tripleBuffer ? 3 : 2;

static constexpr uint32 g_maxViewsPerFrame = 16;
static constexpr uint32 g_maxFramesBeforeDiscard = 4; // number of frames before ViewData is discarded if not written to

static std::atomic_uint g_producerIndex { 0 }; // game thread frame index
static std::atomic_uint g_consumerIndex { 0 }; // render thread frame index

// thread-local frame index for the game and render threads
// @NOTE: thread local so initialized to 0 on each thread by default
thread_local std::atomic_uint* g_threadFrameIndex;

static std::counting_semaphore<g_numFrames> g_fullSemaphore { 0 };
static std::counting_semaphore<g_numFrames> g_freeSemaphore { g_numFrames };

#pragma region ResourceBindings

/// TODO: refactor to use mappings instead of idx (void* directly to element on cpu)
typedef void (*WriteBufferDataFunction)(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

template <class T>
static void OnBindingChanged_Default(T* resource, uint32 prev, uint32 next)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    AssertDebug(resource != nullptr);

    RenderApi_AssignResourceBinding(resource, next);
}

template <class ProxyType>
static void WriteBufferData_Default(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u, "Invalid index for writing buffer data!");

    ProxyType* proxyCasted = static_cast<ProxyType*>(proxy);
    AssertDebug(proxyCasted != nullptr, "Proxy is null!");

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

extern void OnBindingChanged_MeshEntity(Entity* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_MeshEntity(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void OnBindingChanged_AmbientProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next);
extern void WriteBufferData_EnvGrid(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next);
extern void WriteBufferData_Light(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Material(Material* lightmapVolume, uint32 prev, uint32 next);

extern void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next);

struct ResourceBindings
{
    struct SubtypeResourceBindings
    {
        const HypClass* resourceClass;
        GpuBufferHolderBase* gpuBufferHolder;

        // Element binding index to mapping in CPU memory (only if gpuBufferHolder is not null)
        SparsePagedArray<Pair<uint32, void*>, 1024> indexAndMapping;

        SubtypeResourceBindings(const HypClass* resourceClass, GpuBufferHolderBase* gpuBufferHolder)
            : resourceClass(resourceClass),
              gpuBufferHolder(gpuBufferHolder)
        {
            AssertDebug(resourceClass != nullptr);
        }
    };

    SparsePagedArray<SubtypeResourceBindings, 16> subtypeBindings;
    HashMap<TypeId, SubtypeResourceBindings*> cache;

    ResourceBindingAllocator<> meshEntityBindingsAllocator;
    ResourceBinder<Entity, &OnBindingChanged_MeshEntity> meshEntityBinder { &meshEntityBindingsAllocator };

    // Shared index allocator for reflection probes and sky probes.
    ResourceBindingAllocator<g_maxBoundReflectionProbes> reflectionProbeBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> reflectionProbeBinder { &reflectionProbeBindingsAllocator };

    // ambient probes bind to their own slot since they don't set image data
    ResourceBindingAllocator<g_maxBoundAmbientProbes> ambientProbeBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_AmbientProbe> ambientProbeBinder { &ambientProbeBindingsAllocator };

    ResourceBindingAllocator<16> envGridBindingsAllocator;
    ResourceBinder<EnvGrid, &OnBindingChanged_EnvGrid> envGridBinder { &envGridBindingsAllocator };

    ResourceBindingAllocator<> lightBindingsAllocator;
    ResourceBinder<Light, &OnBindingChanged_Light> lightBinder { &lightBindingsAllocator };

    ResourceBindingAllocator<> lightmapVolumeBindingsAllocator;
    ResourceBinder<LightmapVolume, &OnBindingChanged_Default<LightmapVolume>> lightmapVolumeBinder { &lightmapVolumeBindingsAllocator };

    ResourceBindingAllocator<> materialBindingsAllocator;
    ResourceBinder<Material, &OnBindingChanged_Material> materialBinder { &materialBindingsAllocator };

    ResourceBindingAllocator<> textureBindingsAllocator;
    ResourceBinder<Texture, &OnBindingChanged_Texture> textureBinder { &textureBindingsAllocator };

    ResourceBindingAllocator<> skeletonBindingsAllocator;
    ResourceBinder<Skeleton, &OnBindingChanged_Default<Skeleton>> skeletonBinder { &skeletonBindingsAllocator };

    void Assign(HypObjectBase* resource, uint32 binding)
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        AssertDebug(resource != nullptr);

        ObjIdBase id = resource->Id();
        AssertDebug(id.IsValid());

        SubtypeResourceBindings& bindings = GetSubtypeBindings(id.GetTypeId());

        if (bindings.resourceClass == EnvProbe::Class())
        {
            HYP_LOG(Rendering, Debug, "Setting EnvProbe {} binding to {}", resource->Id(), binding);
        }

        if (binding == ~0u)
        {
            bindings.indexAndMapping.EraseAt(id.ToIndex());

            return;
        }

        void* cpuMapping = nullptr;

        if (bindings.gpuBufferHolder != nullptr)
        {
            /// TODO: Move this to somewhere else (like when the binding gets created?)
            bindings.gpuBufferHolder->EnsureCapacity(binding);

            cpuMapping = bindings.gpuBufferHolder->GetCpuMapping(binding);
            AssertDebug(cpuMapping != nullptr);
        }

        bindings.indexAndMapping.Emplace(id.ToIndex(), binding, cpuMapping);
    }

    Pair<uint32, void*> Retrieve(const HypObjectBase* resource) const
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        return resource ? Retrieve(resource->Id()) : Pair<uint32, void*>(~0u, nullptr);
    }

    Pair<uint32, void*> Retrieve(ObjIdBase id) const
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        if (!id.IsValid())
        {
            return Pair<uint32, void*>(~0u, nullptr); // invalid resource
        }

        const SubtypeResourceBindings& bindings = const_cast<ResourceBindings*>(this)->GetSubtypeBindings(id.GetTypeId());

        const auto* elem = bindings.indexAndMapping.TryGet(id.ToIndex());

        AssertDebug(elem != nullptr, "Failed to retrieve resource binding for resource with ID: {} for frame {}!", id, RenderApi_GetFrameIndex_RenderThread());

        return elem ? *elem : Pair<uint32, void*>(~0u, nullptr);
    }

    SubtypeResourceBindings& GetSubtypeBindings(TypeId typeId)
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        auto cacheIt = cache.Find(typeId);
        if (cacheIt != cache.End())
        {
            return *cacheIt->second;
        }

        const HypClass* hypClass = GetClass(typeId);
        AssertDebug(hypClass != nullptr, "TypeId {} does not have a HypClass!", typeId.Value());

        int staticIndex = -1;

        do
        {
            staticIndex = hypClass->GetStaticIndex();
            AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *hypClass->GetName());

            if (SubtypeResourceBindings* bindings = subtypeBindings.TryGet(staticIndex))
            {
                cache[typeId] = bindings;

                return *bindings;
            }

            hypClass = hypClass->GetParent();
        }
        while (hypClass);

        HYP_FAIL("No SubtypeBindings container found for TypeId %u (HypClass: %s). Missing DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?", typeId.Value(), *GetClass(typeId)->GetName());
    }
};

#pragma endregion ResourceBindings

#pragma region ResourceContainer

class NullProxy : public IRenderProxy
{
    char bufferData[1];
};

struct RenderProxyAllocator
{
    SizeType classSize = 0;
    SizeType classAlignment = 0;
    DynamicAllocator allocator;

    void* Alloc()
    {
        if (classSize == 0 || classAlignment == 0)
        {
            return nullptr;
        }

        return allocator.Allocate(classSize, classAlignment);
    }

    void Free(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        allocator.Free(ptr);
    }
};

/*! \brief Holds points to used resources and use count (for rendering).
 *  When the use count reaches zero, it is marked for deferred deletion from
 *  the render thread to ensure no frames are currently using it for rendering as it gets deleted */
struct ResourceData final
{
    HypObjectBase* resource;
    AtomicVar<uint32> count;

    ResourceData(HypObjectBase* resource)
        : resource(resource),
          count(0)
    {
        AssertDebug(resource);

        resource->GetObjectHeader_Internal()->IncRefStrong();
    }

    ResourceData(const ResourceData& other) = delete;
    ResourceData& operator=(const ResourceData& other) = delete;

    ResourceData(ResourceData&& other) noexcept = delete;
    ResourceData& operator=(ResourceData&& other) noexcept = delete;

    ~ResourceData()
    {
        if (resource)
        {
            resource->GetObjectHeader_Internal()->DecRefStrong();
        }
    }
};

struct ResourceSubtypeData final
{
    static constexpr uint32 poolPageSize = 1024;

    TypeId typeId;

    // Map from id -> ResourceData
    SparsePagedArray<ResourceData, 256> data;

    Bitset indicesPendingDelete;
    Bitset indicesPendingUpdate;

    ResourceBinderBase* resourceBinder;
    GpuBufferHolderBase* gpuBufferHolder;

    WriteBufferDataFunction writeBufferDataFn;

    // == optional render proxy data ==
    RenderProxyAllocator proxyAllocator;
    SparsePagedArray<IRenderProxy*, 256> proxies;
    IRenderProxy* (*constructProxyFn)(void* ptr);
    void (*assignProxyFn)(IRenderProxy* dst, const IRenderProxy* src);
    bool hasProxyData : 1;

    template <class ResourceType, class ProxyType>
    ResourceSubtypeData(
        TypeWrapper<ResourceType>,
        TypeWrapper<ProxyType>,
        GpuBufferHolderBase* gpuBufferHolder = nullptr,
        ResourceBinderBase* resourceBinder = nullptr,
        WriteBufferDataFunction writeBufferDataFn = nullptr)
        : typeId(TypeId::ForType<ResourceType>()),
          hasProxyData(false),
          constructProxyFn(nullptr),
          assignProxyFn(nullptr),
          gpuBufferHolder(gpuBufferHolder),
          resourceBinder(resourceBinder),
          writeBufferDataFn(writeBufferDataFn)
    {
        // if ProxyType != NullProxy then we setup proxy pool
        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            hasProxyData = true;

            proxyAllocator.classSize = sizeof(ProxyType);
            proxyAllocator.classAlignment = alignof(ProxyType);

            constructProxyFn = [](void* ptr) -> IRenderProxy*
            {
                return new (ptr) ProxyType;
            };

            assignProxyFn = [](IRenderProxy* dst, const IRenderProxy* src)
            {
                AssertDebug(dst != nullptr && src != nullptr);

                ProxyType* dstCasted = static_cast<ProxyType*>(dst);
                const ProxyType* srcCasted = static_cast<const ProxyType*>(src);

                *dstCasted = *srcCasted;
            };

            // set the WriteBufferData function pointer to some default if one has not been provided
            if (!writeBufferDataFn)
            {
                this->writeBufferDataFn = &WriteBufferData_Default<ProxyType>;
            }
        }
    }

    ResourceSubtypeData(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData& operator=(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData(ResourceSubtypeData&& other) noexcept = default;
    ResourceSubtypeData& operator=(ResourceSubtypeData&& other) noexcept = default;

    ~ResourceSubtypeData()
    {
        if (hasProxyData)
        {
            for (IRenderProxy* proxy : proxies)
            {
                proxy->~IRenderProxy();

                proxyAllocator.Free(proxy);
            }

            proxies.Clear();
        }

        AssertDebug(proxies.Empty());
    }

    HYP_FORCE_INLINE void SetGpuElem(uint32 idx, IRenderProxy* proxy)
    {
        AssertDebug(writeBufferDataFn != nullptr);
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(idx != ~0u);

        writeBufferDataFn(gpuBufferHolder, idx, proxy);
    }
};

struct ResourceContainer
{
    ResourceSubtypeData& GetSubtypeData(TypeId typeId)
    {
        auto cacheIt = cache.Find(typeId);
        if (cacheIt != cache.End())
        {
            return *cacheIt->second;
        }

        const HypClass* hypClass = GetClass(typeId);
        AssertDebug(hypClass != nullptr, "TypeId {} does not have a HypClass!", typeId.Value());

        int staticIndex = -1;

        do
        {
            staticIndex = hypClass->GetStaticIndex();
            AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *hypClass->GetName());

            if (ResourceSubtypeData* subtypeData = dataByType.TryGet(staticIndex))
            {
                // found the subtype data for this typeId - cache it for O(1) retrieval next time
                cache[typeId] = subtypeData;

                return *subtypeData;
            }

            hypClass = hypClass->GetParent();
        }
        while (hypClass);

        HYP_FAIL("No SubtypeData container found for TypeId %u (HypClass: %s)! Missing DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?", typeId.Value(), *GetClass(typeId)->GetName());
    }

    SparsePagedArray<ResourceSubtypeData, 16> dataByType;
    HashMap<TypeId, ResourceSubtypeData*> cache;
};

struct ResourceContainerFactoryRegistry
{
    Array<Proc<void(ResourceBindings&, ResourceContainer&)>> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry instance;
        return instance;
    }

    void InvokeAll(ResourceBindings& resourceBindings, ResourceContainer& resourceContainer)
    {
        for (auto& func : funcs)
        {
            func(resourceBindings, resourceContainer);
        }
    }
};
template <class ResourceType, class ProxyType>
struct ResourceContainerFactory
{
    static constexpr TypeId typeId = TypeId::ForType<ResourceType>();

    static const HypClass* GetResourceClass()
    {
        return GetClass(typeId);
    }

    template <class ResourceBinderType>
    ResourceContainerFactory(
        GlobalRenderBuffer buf,
        ResourceBinderType ResourceBindings::* memResourceBinder,
        WriteBufferDataFunction writeBufferDataFn = nullptr)
    {
        ResourceContainerFactoryRegistry::GetInstance()
            .funcs.PushBack([=](ResourceBindings& resourceBindings, ResourceContainer& container)
                {
                    const HypClass* resourceClass = GetResourceClass();
                    AssertDebug(resourceClass != nullptr, "Class not found for TypeId '{}'!", typeId.Value());

                    const int staticIndex = resourceClass->GetStaticIndex();
                    AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *resourceClass->GetName());

                    GpuBufferHolderBase* gpuBufferHolder = buf < GRB_MAX ? g_renderGlobalState->gpuBuffers[buf] : nullptr;
                    ResourceBinderType* resourceBinder = memResourceBinder ? &(resourceBindings.*memResourceBinder) : nullptr;

                    if (!resourceBindings.subtypeBindings.HasIndex(staticIndex))
                    {
                        // add new ResourceSubtypeBindings slot for the given class
                        resourceBindings.subtypeBindings.Emplace(staticIndex, resourceClass, gpuBufferHolder);
                    }

                    AssertDebug(!container.dataByType.HasIndex(staticIndex),
                        "SubtypeData container already exists for TypeId {} (HypClass: {})! Duplicate DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?",
                        typeId.Value(), *GetClass(typeId)->GetName());

                    container.dataByType.Emplace(
                        staticIndex,
                        TypeWrapper<ResourceType>(),
                        TypeWrapper<ProxyType>(),
                        gpuBufferHolder,
                        resourceBinder,
                        writeBufferDataFn);
                });
    }
};

#define DECLARE_RENDER_DATA_CONTAINER(resourceType, proxyType, ...) \
    static ResourceContainerFactory<class resourceType, class proxyType> g_##resourceType##_container_factory { __VA_ARGS__ };

#pragma endregion ResourceContainer

struct ViewData
{
    RenderProxyList* renderProxyList = nullptr;
    uint32 framesSinceWrite : 4 = 0; // framesSinceWrite of this view in the current frame, used to determine if the view is still alive if not reset to zero.
};

struct FrameData
{
    LinkedList<ViewData> perViewData;
    HashMap<View*, ViewData*> views;
    ResourceContainer resources;
};

static FrameData g_frameData[g_numFrames];

HYP_API void RenderApi_Init()
{
    Threads::AssertOnThread(g_mainThread);

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();

    for (uint32 i = 0; i < g_numFrames; i++)
    {
        registry.InvokeAll(*g_renderGlobalState->resourceBindings, g_frameData[i].resources);
    }

    registry.funcs.Clear();

    g_renderGlobalState->materialDescriptorSetManager->CreateFallbackMaterialDescriptorSet();
}

HYP_API uint32 RenderApi_GetFrameIndex_RenderThread()
{
    return g_consumerIndex.load(std::memory_order_relaxed);
}

HYP_API uint32 RenderApi_GetFrameIndex_GameThread()
{
    return g_producerIndex.load(std::memory_order_relaxed);
}

HYP_API RenderProxyList& RenderApi_GetProducerProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    const uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    ViewData*& vd = g_frameData[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frameData[slot].perViewData.EmplaceBack();

        vd->renderProxyList = view->GetRenderProxyList(slot);
        AssertDebug(vd->renderProxyList != nullptr);
    }

    // reset the framesSinceWrite counter
    vd->framesSinceWrite = 0;

    return *vd->renderProxyList;
}

HYP_API RenderProxyList& RenderApi_GetConsumerProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    const uint32 slot = g_consumerIndex.load(std::memory_order_relaxed);

    ViewData*& vd = g_frameData[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frameData[slot].perViewData.EmplaceBack();

        vd->renderProxyList = view->GetRenderProxyList(slot);
        AssertDebug(vd->renderProxyList != nullptr);
    }

    return *vd->renderProxyList;
}

HYP_API uint32 RenderApi_AddRef(HypObjectBase* resource)
{
    if (!resource)
    {
        return 0;
    }

    Threads::AssertOnThread(g_gameThread);

    const ObjIdBase resourceId = resource->Id();

    const uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& fd = g_frameData[slot];

    ResourceSubtypeData& subtypeData = fd.resources.GetSubtypeData(resourceId.GetTypeId());
    ResourceData* rd = subtypeData.data.TryGet(resourceId.ToIndex());

    if (!rd)
    {
        rd = &*subtypeData.data.Emplace(resourceId.ToIndex(), resource);
    }

    uint32 count = rd->count.Increment(1, MemoryOrder::RELAXED);

    subtypeData.indicesPendingDelete.Set(resourceId.ToIndex(), false);

    return count + 1;
}

HYP_API uint32 RenderApi_ReleaseRef(ObjIdBase id)
{
    if (!id.IsValid())
    {
        return ~0u;
    }

    Threads::AssertOnThread(g_gameThread);

    const uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& fd = g_frameData[slot];

    ResourceSubtypeData& subtypeData = fd.resources.GetSubtypeData(id.GetTypeId());
    ResourceData* rd = subtypeData.data.TryGet(id.ToIndex());

    if (!rd)
    {
        return ~0u; // no ref count for this resource
    }

    uint32 count;

    if ((count = rd->count.Decrement(1, MemoryOrder::RELAXED)) == 1)
    {
        subtypeData.indicesPendingDelete.Set(id.ToIndex(), true);
    }

    return count - 1;
}

HYP_API void RenderApi_UpdateRenderProxy(ObjIdBase id)
{
    if (!id.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(g_gameThread);

    const uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& fd = g_frameData[slot];

    ResourceSubtypeData& subtypeData = fd.resources.GetSubtypeData(id.GetTypeId());
    ResourceData& data = subtypeData.data.Get(id.ToIndex());

    AssertDebug(data.count.Get(MemoryOrder::RELAXED), "expected ref count to be > 0 when calling UpdateRenderProxy()");
    AssertDebug(!subtypeData.indicesPendingDelete.Test(id.ToIndex()), "Why is it marked for delete?");

    AssertDebug(subtypeData.hasProxyData, "Cannot use UpdateRenderProxy() for type which does not have proxy data! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    IRenderProxy* proxy;

    if (IRenderProxy** p = subtypeData.proxies.TryGet(id.ToIndex()))
    {
        proxy = *p;
    }
    else
    {
        void* ptr = subtypeData.proxyAllocator.Alloc();
        AssertDebug(ptr != nullptr, "Failed to allocate render proxy!");

        // construct proxy object
        AssertDebug(subtypeData.constructProxyFn != nullptr);
        proxy = subtypeData.constructProxyFn(ptr);

        subtypeData.proxies.Emplace(id.ToIndex(), proxy);

        HYP_LOG(Rendering, Debug, "Created render proxy for resource id {} of type {} at index {} for frame {}",
            id, GetClass(subtypeData.typeId)->GetName(), id.ToIndex(), slot);
    }

    AssertDebug(proxy);
    AssertDebug(data.resource);

    if (data.resource->InstanceClass()->IsDerivedFrom(RenderProxyable::Class()))
    {
        RenderProxyable* renderProxyable = static_cast<RenderProxyable*>(data.resource);
        renderProxyable->UpdateRenderProxy(proxy);

        // mark for buffer data update from render thread
        subtypeData.indicesPendingUpdate.Set(id.ToIndex(), true);
    }
    else
    {
        HYP_LOG(Rendering, Warning, "UpdateRenderProxy called for resource id {} of type {} which is not a RenderProxyable! Skipping proxy update.\n",
            id, GetClass(subtypeData.typeId)->GetName());
    }
}

HYP_API void RenderApi_UpdateRenderProxy(ObjIdBase id, const IRenderProxy* srcProxy)
{
    if (!id.IsValid())
    {
        return;
    }

    AssertDebug(srcProxy != nullptr);

    Threads::AssertOnThread(g_gameThread);

    const uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& fd = g_frameData[slot];

    ResourceSubtypeData& subtypeData = fd.resources.GetSubtypeData(id.GetTypeId());
    ResourceData& data = subtypeData.data.Get(id.ToIndex());

    AssertDebug(data.count.Get(MemoryOrder::RELAXED), "expected ref count to be > 0 when calling UpdateRenderProxy()");
    AssertDebug(!subtypeData.indicesPendingDelete.Test(id.ToIndex()), "Why is it marked for delete?");

    AssertDebug(subtypeData.hasProxyData, "Cannot use UpdateResource() for type which does not have proxy data! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    IRenderProxy* dstProxy;

    if (IRenderProxy** p = subtypeData.proxies.TryGet(id.ToIndex()))
    {
        dstProxy = *p;
    }
    else
    {
        void* ptr = subtypeData.proxyAllocator.Alloc();
        AssertDebug(ptr != nullptr, "Failed to allocate render proxy!");

        // construct proxy object
        dstProxy = subtypeData.constructProxyFn(ptr);

        subtypeData.proxies.Emplace(id.ToIndex(), dstProxy);

        HYP_LOG(Rendering, Debug, "Created render proxy for resource id {} of type {} at index {} for frame {}",
            id, GetClass(subtypeData.typeId)->GetName(), id.ToIndex(), slot);
    }

    subtypeData.assignProxyFn(dstProxy, srcProxy);

    if (data.resource->InstanceClass()->IsDerivedFrom(RenderProxyable::Class()))
    {
        RenderProxyable* renderProxyable = static_cast<RenderProxyable*>(data.resource);
        renderProxyable->UpdateRenderProxy(dstProxy);

        // mark for buffer data update from render thread
        subtypeData.indicesPendingUpdate.Set(id.ToIndex(), true);
    }
    else
    {
        HYP_LOG(Rendering, Warning, "UpdateRenderProxy called for resource id {} of type {} which is not a RenderProxyable! Skipping proxy update.\n",
            id, GetClass(subtypeData.typeId)->GetName());
    }
}

HYP_API IRenderProxy* RenderApi_GetRenderProxy(ObjIdBase id)
{
    AssertDebug(id.IsValid());
    AssertDebug(id.GetTypeId() != TypeId::Void());

    Threads::AssertOnThread(g_renderThread);

    uint32 slot = g_consumerIndex.load(std::memory_order_relaxed);

    FrameData& frameData = g_frameData[slot];

    ResourceSubtypeData& subtypeData = frameData.resources.GetSubtypeData(id.GetTypeId());
    AssertDebug(subtypeData.hasProxyData, "Cannot use GetRenderProxy() for type which does not have a RenderProxy! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    if (!subtypeData.proxies.HasIndex(id.ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "No render proxy found for resource: {} in frame {}", id, slot);

        return nullptr; // no proxy for this resource
    }

    IRenderProxy* proxy = subtypeData.proxies.Get(id.ToIndex());
    AssertDebug(proxy != nullptr);

    return proxy;
}

HYP_API void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_renderGlobalState->resourceBindings->Assign(resource, binding);
}

HYP_API uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource)
{
#ifdef HYP_DEBUG_MODE
    // FIXME: Add better check to ensure it is from a render task thread.
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    return g_renderGlobalState->resourceBindings->Retrieve(resource).first;
}

HYP_API uint32 RenderApi_RetrieveResourceBinding(ObjIdBase id)
{
#ifdef HYP_DEBUG_MODE
    // FIXME: Add better check to ensure it is from a render task thread.
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    return g_renderGlobalState->resourceBindings->Retrieve(id).first;
}

HYP_API void RenderApi_BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_threadFrameIndex = &g_producerIndex;

    g_freeSemaphore.acquire();

    uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& frameData = g_frameData[slot];

    for (auto it = frameData.perViewData.Begin(); it != frameData.perViewData.End(); ++it)
    {
        ViewData& vd = *it;

        AssertDebug(vd.renderProxyList != nullptr);
    }
}

HYP_API void RenderApi_EndFrame_GameThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread);
#endif

    uint32 slot = g_producerIndex.load(std::memory_order_relaxed);

    FrameData& frameData = g_frameData[slot];

    for (auto it = frameData.perViewData.Begin(); it != frameData.perViewData.End(); ++it)
    {
        ViewData& vd = *it;

        AssertDebug(vd.renderProxyList != nullptr);
    }

    g_producerIndex.store((g_producerIndex.load(std::memory_order_relaxed) + 1) % g_numFrames, std::memory_order_relaxed);

    g_fullSemaphore.release();
}

static void RenderApi_UpdateBoundResources()
{
}

HYP_API void RenderApi_BeginFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_threadFrameIndex = &g_consumerIndex;

    g_fullSemaphore.acquire();

    uint32 slot = g_consumerIndex.load(std::memory_order_relaxed);

    FrameData& frameData = g_frameData[slot];

    HYPERION_ASSERT_RESULT(RenderCommands::Flush());

    for (auto it = frameData.perViewData.Begin(); it != frameData.perViewData.End(); ++it)
    {
        ViewData& vd = *it;
        AssertDebug(vd.renderProxyList != nullptr);
    }

    for (ResourceSubtypeData& subtypeData : frameData.resources.dataByType)
    {
        if (subtypeData.resourceBinder)
        {
            for (ResourceData& elem : subtypeData.data)
            {
                AssertDebug(elem.resource != nullptr);
                subtypeData.resourceBinder->Consider(elem.resource);
            }
        }
    }

    // assign the actual bindings:
    /// TODO: This should be done in the ResourceBinder itself, not here.
    g_renderGlobalState->resourceBindings->meshEntityBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->ambientProbeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->reflectionProbeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->envGridBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->lightBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->lightmapVolumeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->materialBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->textureBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->skeletonBinder.ApplyUpdates();

    HYP_LOG(Rendering, Debug, "Mesh entities: {} bound", g_renderGlobalState->resourceBindings->meshEntityBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Ambient probes: {} bound", g_renderGlobalState->resourceBindings->ambientProbeBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Reflection probes: {} bound", g_renderGlobalState->resourceBindings->reflectionProbeBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Env grids: {} bound", g_renderGlobalState->resourceBindings->envGridBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Lights: {} bound", g_renderGlobalState->resourceBindings->lightBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Lightmap volumes: {} bound", g_renderGlobalState->resourceBindings->lightmapVolumeBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Materials: {} bound", g_renderGlobalState->resourceBindings->materialBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Textures: {} bound", g_renderGlobalState->resourceBindings->textureBinder.TotalBoundResources());
    HYP_LOG(Rendering, Debug, "Skeletons: {} bound", g_renderGlobalState->resourceBindings->skeletonBinder.TotalBoundResources());

    for (ResourceSubtypeData& subtypeData : frameData.resources.dataByType)
    {
        if (subtypeData.indicesPendingUpdate.Count() != 0)
        {
            AssertDebug(subtypeData.resourceBinder != nullptr);

            const Bitset& currentBoundIndices = subtypeData.resourceBinder->GetBoundIndices(subtypeData.typeId);

            if (currentBoundIndices.Count() == 0)
            {
                // early out; nothing is bound.
                continue;
            }

            // Handle proxies that were updated on game thread
            for (Bitset::BitIndex i = subtypeData.indicesPendingUpdate.FirstSetBitIndex();
                i != Bitset::notFound;
                i = subtypeData.indicesPendingUpdate.NextSetBitIndex(i + 1))
            {
                if (!currentBoundIndices.Test(i))
                {
                    continue;
                }

                const ObjIdBase resourceId = ObjIdBase(subtypeData.typeId, uint32(i + 1));

                AssertDebug(subtypeData.hasProxyData);
                AssertDebug(subtypeData.writeBufferDataFn != nullptr);

                const Pair<uint32, void*> bindingData = g_renderGlobalState->resourceBindings->Retrieve(resourceId);
                AssertDebug(bindingData.first != ~0u && bindingData.second != nullptr, "Failed to retrieve binding for resource: {} in frame {}, but it is marked as bound (index: {})",
                    resourceId, slot, i);

                IRenderProxy* proxy = subtypeData.proxies.Get(i);
                AssertDebug(proxy != nullptr);

                subtypeData.SetGpuElem(bindingData.first, proxy);

                subtypeData.indicesPendingUpdate.Set(i, false);
            }
        }
    }
}

HYP_API void RenderApi_EndFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    uint32 slot = g_consumerIndex.load(std::memory_order_relaxed);

    FrameData& frameData = g_frameData[slot];

    // cull ViewData that hasn't been written to for a while
    for (auto it = frameData.perViewData.Begin(); it != frameData.perViewData.End();)
    {
        ViewData& vd = *it;
        AssertDebug(vd.renderProxyList != nullptr);

        vd.renderProxyList->RemoveEmptyRenderGroups();

        // Clear out data for views that haven't been written to for a while
        if (vd.framesSinceWrite == g_maxFramesBeforeDiscard)
        {
            auto viewIt = frameData.views.FindIf([&vd](const auto& pair)
                {
                    return pair.second == &vd;
                });

            AssertDebug(viewIt != frameData.views.End());

            frameData.views.Erase(viewIt);

            it = frameData.perViewData.Erase(it);

            continue;
        }

        ++it;
    }

    for (ResourceSubtypeData& subtypeData : frameData.resources.dataByType)
    {
        // @TODO: for deletion, have a fixed number to iterate over per frame so we don't spend too much time on it.
        // Remove resources pending deletion via SafeRelease() for indices marked for deletion from the game thread
        for (Bitset::BitIndex i : subtypeData.indicesPendingDelete)
        {
            ResourceData& rd = subtypeData.data.Get(i);
            AssertDebug(rd.resource != nullptr);
            AssertDebug(rd.count.Get(MemoryOrder::RELAXED) == 0, "Ref count should be 0 before deletion");

            // if we delete it, we want to make sure it is not in marked for update state (don't want to iterate over dead items)
            subtypeData.indicesPendingUpdate.Set(i, false);

            subtypeData.resourceBinder->Deconsider(rd.resource);

            // Swap refcount owner over to the Handle
            AnyHandle resource { rd.resource };
            subtypeData.data.EraseAt(i);

            if (subtypeData.hasProxyData)
            {
                AssertDebug(subtypeData.proxies.HasIndex(i), "proxy missing at index: {}", i);

                IRenderProxy* proxy = subtypeData.proxies.Get(i);
                AssertDebug(proxy != nullptr);

                HYP_LOG(Rendering, Debug, "Deleting render proxy for resource id {} at index {} for frame {}",
                    resource.Id(), i, slot);

                subtypeData.proxies.EraseAt(i);

                // safely release the proxy's resources before we destruct it:
                proxy->SafeRelease();

                // pool doesnt call destructors so invoke manually
                proxy->~IRenderProxy();

#ifdef HYP_DEBUG_MODE
                // :)
                proxy->state = 0xDEAD;
#endif

                subtypeData.proxyAllocator.Free(proxy);
            }

            // safely release all the held resources:
            if (resource.IsValid())
            {
                g_safeDeleter->SafeRelease(std::move(resource));
            }
        }

        subtypeData.indicesPendingDelete.Clear();
    }

    g_consumerIndex.store((slot + 1) % g_numFrames, std::memory_order_relaxed);

    g_freeSemaphore.release();
}

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : ShadowMapAllocator(MakeUnique<class ShadowMapAllocator>()),
      GpuBufferHolderMap(MakeUnique<class GpuBufferHolderMap>()),
      placeholderData(MakeUnique<PlaceholderData>()),
      resourceBindings(new ResourceBindings()),
      materialDescriptorSetManager(new MaterialDescriptorSetManager()),
      graphicsPipelineCache(new GraphicsPipelineCache()),
      bindlessStorage(new BindlessStorage())
{
    gpuBuffers.buffers[GRB_WORLDS] = GpuBufferHolderMap->GetOrCreate<WorldShaderData, GpuBufferType::CBUFF>();
    gpuBuffers.buffers[GRB_CAMERAS] = GpuBufferHolderMap->GetOrCreate<CameraShaderData, GpuBufferType::CBUFF>();
    gpuBuffers.buffers[GRB_LIGHTS] = GpuBufferHolderMap->GetOrCreate<LightShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENTITIES] = GpuBufferHolderMap->GetOrCreate<EntityShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_MATERIALS] = GpuBufferHolderMap->GetOrCreate<MaterialShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_SKELETONS] = GpuBufferHolderMap->GetOrCreate<SkeletonShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_SHADOW_MAPS] = GpuBufferHolderMap->GetOrCreate<ShadowMapShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENV_PROBES] = GpuBufferHolderMap->GetOrCreate<EnvProbeShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENV_GRIDS] = GpuBufferHolderMap->GetOrCreate<EnvGridShaderData, GpuBufferType::CBUFF>();
    gpuBuffers.buffers[GRB_LIGHTMAP_VOLUMES] = GpuBufferHolderMap->GetOrCreate<LightmapVolumeShaderData, GpuBufferType::SSBO>();

    GlobalDescriptorTable = g_renderBackend->MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

    placeholderData->Create();
    ShadowMapAllocator->Initialize();

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        SetDefaultDescriptorSetElements(frameIndex);
    }

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    GlobalDescriptorTable->Create();

    mainRenderer = new DeferredRenderer();
    mainRenderer->Initialize();

    globalRenderers[GRT_ENV_PROBE].Resize(EPT_MAX);
    globalRenderers[GRT_ENV_PROBE][EPT_REFLECTION] = new ReflectionProbeRenderer();
    globalRenderers[GRT_ENV_PROBE][EPT_SKY] = new ReflectionProbeRenderer();

    globalRenderers[GRT_ENV_GRID].PushBack(new EnvGridRenderer());

    globalRenderers[GRT_SHADOW_MAP].Resize(LT_MAX); // 1 ShadowMapRenderer per LightType
}

RenderGlobalState::~RenderGlobalState()
{
    delete resourceBindings;

    bindlessStorage->UnsetAllResources();
    delete bindlessStorage;
    bindlessStorage = nullptr;

    ShadowMapAllocator->Destroy();
    GlobalDescriptorTable->Destroy();
    placeholderData->Destroy();

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        for (uint32 j = 0; j < globalRenderers[i].Size(); j++)
        {
            if (globalRenderers[i][j])
            {
                globalRenderers[i][j]->Shutdown();
                delete globalRenderers[i][j];
            }
        }
    }

    delete materialDescriptorSetManager;
    materialDescriptorSetManager = nullptr;

    delete graphicsPipelineCache;
    graphicsPipelineCache = nullptr;

    mainRenderer->Shutdown();
    delete mainRenderer;
    mainRenderer = nullptr;
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    for (auto& it : GpuBufferHolderMap->GetItems())
    {
        it.second->UpdateBufferSize(frame->GetFrameIndex());
        it.second->UpdateBufferData(frame->GetFrameIndex());
    }
}

void RenderGlobalState::AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(!globalRenderers[globalRendererType].Contains(renderer));

    globalRenderers[globalRendererType].PushBack(renderer);
}

void RenderGlobalState::RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(globalRenderers[globalRendererType].Contains(renderer));

    delete renderer;

    globalRenderers[globalRendererType].Erase(renderer);
}

void RenderGlobalState::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    static_assert(sizeof(BlueNoiseBuffer::sobol256spp256d) == sizeof(BlueNoise::sobol256spp256d));
    static_assert(sizeof(BlueNoiseBuffer::scramblingTile) == sizeof(BlueNoise::scramblingTile));
    static_assert(sizeof(BlueNoiseBuffer::rankingTile) == sizeof(BlueNoise::rankingTile));

    constexpr SizeType blueNoiseBufferSize = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol256spp256dOffset = offsetof(BlueNoiseBuffer, sobol256spp256d);
    constexpr SizeType sobol256spp256dSize = sizeof(BlueNoise::sobol256spp256d);
    constexpr SizeType scramblingTileOffset = offsetof(BlueNoiseBuffer, scramblingTile);
    constexpr SizeType scramblingTileSize = sizeof(BlueNoise::scramblingTile);
    constexpr SizeType rankingTileOffset = offsetof(BlueNoiseBuffer, rankingTile);
    constexpr SizeType rankingTileSize = sizeof(BlueNoise::rankingTile);

    static_assert(blueNoiseBufferSize == (sobol256spp256dOffset + sobol256spp256dSize) + ((scramblingTileOffset - (sobol256spp256dOffset + sobol256spp256dSize)) + scramblingTileSize) + ((rankingTileOffset - (scramblingTileOffset + scramblingTileSize)) + rankingTileSize));

    GpuBufferRef blueNoiseBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(BlueNoiseBuffer));
    HYPERION_ASSERT_RESULT(blueNoiseBuffer->Create());
    blueNoiseBuffer->Copy(sobol256spp256dOffset, sobol256spp256dSize, &BlueNoise::sobol256spp256d[0]);
    blueNoiseBuffer->Copy(scramblingTileOffset, scramblingTileSize, &BlueNoise::scramblingTile[0]);
    blueNoiseBuffer->Copy(rankingTileOffset, rankingTileSize, &BlueNoise::rankingTile[0]);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
            ->SetElement(NAME("BlueNoiseBuffer"), blueNoiseBuffer);
    }
}

void RenderGlobalState::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    GpuBufferRef sphereSamplesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(Vec4f) * 4096);
    HYPERION_ASSERT_RESULT(sphereSamplesBuffer->Create());

    Vec4f* sphereSamples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(Vec3f {
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed) });

        sphereSamples[i] = Vec4f(sample, 0.0f);
    }

    sphereSamplesBuffer->Copy(sizeof(Vec4f) * 4096, sphereSamples);

    delete[] sphereSamples;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
            ->SetElement(NAME("SphereSamplesBuffer"), sphereSamplesBuffer);
    }
}

void RenderGlobalState::SetDefaultDescriptorSetElements(uint32 frameIndex)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Global
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("WorldsBuffer"), gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("LightsBuffer"), gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("CurrentLight"), gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("ObjectsBuffer"), gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("CamerasBuffer"), gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("EnvGridsBuffer"), gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("EnvProbesBuffer"), gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("CurrentEnvProbe"), gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("VoxelGridTexture"), placeholderData->GetImageView3D1x1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("LightFieldColorTexture"), placeholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("LightFieldDepthTexture"), placeholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("BlueNoiseBuffer"), GpuBufferRef::Null());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("SphereSamplesBuffer"), GpuBufferRef::Null());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("ShadowMapsTextureArray"), placeholderData->GetImageView2D1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("PointLightShadowMapsTextureArray"), placeholderData->GetImageViewCube1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("ShadowMapsBuffer"), gpuBuffers[GRB_SHADOW_MAPS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("LightmapVolumesBuffer"), gpuBuffers[GRB_LIGHTMAP_VOLUMES]->GetBuffer(frameIndex));

    for (uint32 i = 0; i < g_maxBoundReflectionProbes; i++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("EnvProbeTextures"), i, placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
    }

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIUniforms"), placeholderData->GetOrCreateBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms), true /* exact size */));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIIrradianceTexture"), placeholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIDepthTexture"), placeholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("RTRadianceResultTexture"), placeholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("SamplerNearest"), placeholderData->GetSamplerNearest());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("SamplerLinear"), placeholderData->GetSamplerLinearMipmap());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("UITexture"), placeholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("FinalOutputTexture"), placeholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("ShadowMapsTextureArray"), ShadowMapAllocator->GetAtlasImageView());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("PointLightShadowMapsTextureArray"), ShadowMapAllocator->GetPointLightShadowMapImageView());

    // Object
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frameIndex)->SetElement(NAME("CurrentObject"), gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frameIndex)->SetElement(NAME("MaterialsBuffer"), gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frameIndex)->SetElement(NAME("SkeletonsBuffer"), gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frameIndex)->SetElement(NAME("LightmapVolumeIrradianceTexture"), placeholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frameIndex)->SetElement(NAME("LightmapVolumeRadianceTexture"), placeholderData->GetImageView2D1x1R8());

    // Material
    if (g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        for (uint32 textureIndex = 0; textureIndex < g_maxBindlessResources; textureIndex++)
        {
            GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frameIndex)
                ->SetElement(NAME("Textures"), textureIndex, placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
    else
    {
        for (uint32 textureIndex = 0; textureIndex < g_maxBoundTextures; textureIndex++)
        {
            GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frameIndex)
                ->SetElement(NAME("Textures"), textureIndex, placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
}

#pragma endregion RenderGlobalState

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, GRB_ENTITIES, &ResourceBindings::meshEntityBinder, &WriteBufferData_MeshEntity);
DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &ResourceBindings::envGridBinder, &WriteBufferData_EnvGrid);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflectionProbeBinder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflectionProbeBinder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::ambientProbeBinder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);
DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, GRB_LIGHTMAP_VOLUMES, &ResourceBindings::lightmapVolumeBinder);
DECLARE_RENDER_DATA_CONTAINER(Material, RenderProxyMaterial, GRB_MATERIALS, &ResourceBindings::materialBinder);
DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, GRB_INVALID, &ResourceBindings::textureBinder);
DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, GRB_SKELETONS, &ResourceBindings::skeletonBinder);

} // namespace hyperion
