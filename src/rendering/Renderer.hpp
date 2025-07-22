/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/config/Config.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Span.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/RenderStats.hpp>
#include <rendering/CullData.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderQueue.hpp>

namespace hyperion {

class RenderWorld;
class RenderEnvGrid;
class RenderLight;
class Light;
class EnvProbe;
class EnvGrid;
struct CullData;
struct DrawCall;
struct InstancedDrawCall;
struct PassData;
class RendererBase;
class RenderGroup;
class View;
class IDrawCallCollectionImpl;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering")
struct RendererConfig : public ConfigBase<RendererConfig>
{
    HYP_FIELD(JSONPath = "rt.path_tracing.enabled")
    bool pathTracer = false;

    HYP_FIELD(JSONPath = "rt.reflections.enabled")
    bool raytracingReflections = false;

    HYP_FIELD(JSONPath = "rt.gi.enabled")
    bool raytracingGlobalIllumination = false;

    HYP_FIELD(JSONPath = "hbao.enabled")
    bool hbaoEnabled = false;

    HYP_FIELD(JSONPath = "hbil.enabled")
    bool hbilEnabled = false;

    HYP_FIELD(JSONPath = "ssgi.enabled")
    bool ssgiEnabled = false;

    HYP_FIELD(JSONPath = "env_grid.gi.enabled")
    bool envGridGiEnabled = false;

    HYP_FIELD(JSONPath = "env_grid.reflections.enabled")
    bool envGridRadianceEnabled = false;

    HYP_FIELD(JSONPath = "taa.enabled")
    bool taaEnabled = false;

    virtual ~RendererConfig() override = default;
};

/*! \brief Describes the setup for rendering a frame. All RenderSetups must have a valid RenderWorld set. Passed to almost all Render() functions throughout the renderer.
 *  Most of the time you'll want a RenderSetup with a View as well, but compute-only passes can use a RenderSetup without a view. Use HasView() to check if a view is set. */
struct HYP_API RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    RenderWorld* world;
    View* view;
    EnvProbe* envProbe;
    EnvGrid* envGrid;
    Light* light;
    PassData* passData;

private:
    // Private constructor for null RenderSetup
    RenderSetup()
        : world(nullptr),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
    }

public:
    RenderSetup(RenderWorld* world)
        : world(world),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid RenderWorld");
    }

    RenderSetup(RenderWorld* world, View* view)
        : world(world),
          view(view),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid RenderWorld");
    }

    RenderSetup(const RenderSetup& other) = default;
    RenderSetup& operator=(const RenderSetup& other) = default;

    RenderSetup(RenderSetup&& other) noexcept = default;
    RenderSetup& operator=(RenderSetup&& other) noexcept = default;

    ~RenderSetup() = default;

    /*! \see IsValid() */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    /*! \see IsValid() */
    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    /*! \brief Returns true if this RenderSetup is useable for rendering.
     *  This means it has a valid RenderWorld set. If you need to check if a View is set, use HasView() instead. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return world != nullptr;
    }

    /*! \brief Returns true if this RenderSetup has a valid View set. */
    HYP_FORCE_INLINE bool HasView() const
    {
        return view != nullptr;
    }
};

/*! \brief Special null RenderSetup that can be used for simple rendering tasks that don't make sense to use a RenderWorld, such as rendering texture mipmaps.
 *  \internal Use sparingly as most rendering tasks should have a valid RenderWorld and using this will cause the IsValid() check to return false */
extern const RenderSetup& NullRenderSetup();

struct PassDataExt
{
    TypeId typeId;

    PassDataExt()
        : typeId(TypeId::Void())
    {
    }
    virtual ~PassDataExt() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        constexpr TypeId invalidTypeId = TypeId::Void();

        return typeId != invalidTypeId;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE OtherPassDataExt* AsType()
    {
        constexpr TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<OtherPassDataExt*>(this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE const OtherPassDataExt* AsType() const
    {
        constexpr TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<const OtherPassDataExt*>(this);
    }

    // Create a new instance of this PassDataExt (caller owns the allocation)
    virtual PassDataExt* Clone() = 0;

protected:
    PassDataExt(TypeId subtype)
        : typeId(subtype)
    {
    }
};

/*! \brief Data and passes used for rendering a View in the Deferred Renderer. */
struct HYP_API PassData
{
    struct RenderGroupCacheEntry
    {
        WeakHandle<RenderGroup> renderGroup;
        GraphicsPipelineRef graphicsPipeline;
    };

    WeakHandle<View> view;
    Viewport viewport;

    // per-View descriptor sets
    FixedArray<DescriptorSetRef, g_framesInFlight> descriptorSets;

    CullData cullData;

    // cached by ObjId<RenderGroup>
    SparsePagedArray<RenderGroupCacheEntry, 32> renderGroupCache;
    // iterator for removing cache data over frames
    typename SparsePagedArray<RenderGroupCacheEntry, 32>::Iterator renderGroupCacheIterator;

    PassDataExt* next = nullptr;

    virtual ~PassData();

    /*! \brief Safely remove unused graphics pipelines that are no longer used from the cache.
     *  A graphics pipeline is considered unused if the RenderGroup it is associated with has no more references remaining
     *  \param maxIter The maximum number of graphics pipelines to iterate over for this frame.
     *  \returns The number of graphics pipelines that were culled */
    int CullUnusedGraphicsPipelines(int maxIter = 10);

    static GraphicsPipelineRef CreateGraphicsPipeline(
        PassData* pd,
        const ShaderRef& shader,
        const RenderableAttributeSet& renderableAttributes,
        const DescriptorTableRef& descriptorTable = DescriptorTableRef::Null(),
        IDrawCallCollectionImpl* impl = nullptr); // @TODO: Make this param part of renderable attributes
};

class HYP_API RendererBase
{
public:
    virtual ~RendererBase();

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) = 0;

    /*! \brief Cleans up data no longer used for rendering, amortised.
     *  Returns number of cleanup iterations used by this execution */
    virtual int RunCleanupCycle(int maxIter = 10);

protected:
    RendererBase();

    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) = 0;

    PassData* TryGetViewPassData(View* view);
    PassData* FetchViewPassData(View* view, PassDataExt* ext = nullptr);

private:
    SparsePagedArray<PassData*, 16> m_viewPassData;
    typename SparsePagedArray<PassData*, 16>::Iterator m_viewPassDataCleanupIterator;
};

} // namespace hyperion
