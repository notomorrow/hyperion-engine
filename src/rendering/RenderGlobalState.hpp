/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

#include <rendering/Buffers.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <rendering/util/ResourceBinder.hpp>

#include <util/ResourceTracker.hpp>

namespace hyperion {

class Engine;
class Entity;
class ShadowMapAllocator;
class GpuBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class RenderView;
class View;
class DrawCallCollection;
class RendererBase;
class IRenderProxy;
class EnvProbeRenderer;
class EnvProbe;
class ReflectionProbe;
class SkyProbe;
class RenderGlobalState;
class RenderResourceLock;
class UIRenderer;
class MaterialDescriptorSetManager;
class GraphicsPipelineCache;
class BindlessStorage;
class RenderProxyable;

HYP_API extern SizeType GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

// Call at start of engine before render / game thread start ticking.
// Allocates containers declared in RenderGlobalState.cpp via DECLARE_RENDER_DATA_CONTAINER
HYP_API extern void RenderApi_Init();

HYP_API extern uint32 RenderApi_GetFrameIndex_RenderThread();
HYP_API extern uint32 RenderApi_GetFrameIndex_GameThread();

HYP_API extern void RenderApi_BeginFrame_GameThread();
HYP_API extern void RenderApi_EndFrame_GameThread();

HYP_API extern void RenderApi_BeginFrame_RenderThread();
HYP_API extern void RenderApi_EndFrame_RenderThread();

/*! \brief Get the RenderProxyList for the Game thread to write to for the current frame, for the given view.
 *  The game thread adds proxies of entities, lights, envprobes, etc. to this list, which the render thread will
 *  use when rendering the frame.
 *  \note This is only valid to call from the game thread, or from a task that is initiated by the game thread. */
HYP_API extern RenderProxyList& RenderApi_GetProducerProxyList(View* view);

/*! \brief Get the RenderProxyList for the Render thread to read from for the current frame, for the given view.
 *  \note This is only valid to call from the render thread, or from a task that is initiated by the render thread. */
HYP_API extern RenderProxyList& RenderApi_GetConsumerProxyList(View* view);

// Call on game (producer) thread
HYP_API extern uint32 RenderApi_AddRef(HypObjectBase* resource);
HYP_API extern uint32 RenderApi_ReleaseRef(ObjIdBase id);

HYP_API extern void RenderApi_UpdateRenderProxy(ObjIdBase id);
HYP_API extern void RenderApi_UpdateRenderProxy(ObjIdBase id, const IRenderProxy* srcProxy);

// Call on render thread or render thread tasks only (consumer)
HYP_API extern IRenderProxy* RenderApi_GetRenderProxy(ObjIdBase id);

// used on render thread only - assigns all render proxy for the given object to the given binding
HYP_API extern void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding);
// used on render thread only - retrieves the binding set for the given resource (~0u if unset)
HYP_API extern uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource);
HYP_API extern uint32 RenderApi_RetrieveResourceBinding(ObjIdBase id);

/*! \brief Register added/removed/changed resources with the rendering system for the next frame to be rendered */
template <class ElementType>
static inline void RenderApi_UpdateTrackedResources(ResourceTracker<ObjId<ElementType>, ElementType*>& resourceTracker)
{
    // Update refs for materials for this view
    if (auto diff = resourceTracker.GetDiff(); diff.NeedsUpdate())
    {
        static const bool shouldUpdateRenderProxy = IsA<RenderProxyable, ElementType>();

        Array<ObjId<ElementType>> removed;
        resourceTracker.GetRemoved(removed, /* includeChanged */ shouldUpdateRenderProxy);

        Array<ElementType*> added;
        resourceTracker.GetAdded(added, /* includeChanged */ shouldUpdateRenderProxy);

        for (ElementType* elem : added)
        {
            RenderApi_AddRef(elem);

            // Update proxies for changed and added items
            if (shouldUpdateRenderProxy)
            {
                RenderApi_UpdateRenderProxy(elem->Id());
            }
        }

        for (ObjId<ElementType> id : removed)
        {
            RenderApi_ReleaseRef(id);
        }
    }
}

struct ResourceBindings;

enum GlobalRenderBuffer : uint8
{
    GRB_INVALID = uint8(-1),

    GRB_WORLDS = 0,
    GRB_CAMERAS,
    GRB_LIGHTS,
    GRB_ENTITIES,
    GRB_MATERIALS,
    GRB_SKELETONS,
    GRB_SHADOW_MAPS,
    GRB_ENV_PROBES,
    GRB_ENV_GRIDS,
    GRB_LIGHTMAP_VOLUMES,

    GRB_MAX
};

enum GlobalRendererType : uint32
{
    GRT_NONE = ~0u, //<! Not a global renderer type

    GRT_ENV_PROBE = 0, //<! Global renderer instances for different EnvProbe classes
    GRT_ENV_GRID,      //<! Global renderer instance for EnvGrids
    GRT_SHADOW_MAP,    //<! Shadow map renderers, e.g. PointLightShadowRenderer, DirectionalLightShadowRenderer
    GRT_UI,            //<! Globally registered UIRenderer instances to be used by FinalPass to draw the UI onto the backbuffer.

    GRT_MAX
};

struct GlobalGpuBuffers
{
    GpuBufferHolderBase* buffers[GRB_MAX];

    HYP_FORCE_INLINE GpuBufferHolderBase* operator[](GlobalRenderBuffer buf) const
    {
        return buffers[buf];
    }
};

class RenderGlobalState
{
    friend class ResourceBinderBase;

public:
    RenderGlobalState();
    RenderGlobalState(const RenderGlobalState& other) = delete;
    RenderGlobalState& operator=(const RenderGlobalState& other) = delete;
    ~RenderGlobalState();

    void UpdateBuffers(FrameBase* frame);

    void AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);
    void RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer);

    BindlessStorage* bindlessStorage;

    UniquePtr<ShadowMapAllocator> shadowMapAllocator;
    UniquePtr<PlaceholderData> placeholderData;

    UniquePtr<GpuBufferHolderMap> gpuBufferHolders;

    DescriptorTableRef globalDescriptorTable;

    RendererBase* mainRenderer;
    FixedArray<Array<RendererBase*>, GRT_MAX> globalRenderers;

    GlobalGpuBuffers gpuBuffers;
    ResourceBindings* resourceBindings;

    MaterialDescriptorSetManager* materialDescriptorSetManager;

    GraphicsPipelineCache* graphicsPipelineCache;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frameIndex);
};

} // namespace hyperion
