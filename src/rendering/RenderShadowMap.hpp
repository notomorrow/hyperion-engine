/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResource.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Renderer.hpp>

#include <core/math/Matrix4.hpp>
#include <core/math/Vector2.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/IdGenerator.hpp>

#include <util/AtlasPacker.hpp>

#include <Types.hpp>

namespace hyperion {

class FullScreenPass;
class RenderWorld;
class RenderShadowMap;

HYP_ENUM()
enum ShadowMapFilter : uint32
{
    SMF_STANDARD = 0,
    SMF_PCF,
    SMF_CONTACT_HARDENED,
    SMF_VSM,

    SMF_MAX
};

HYP_ENUM()
enum ShadowMapType : uint32
{
    SMT_DIRECTIONAL,
    SMT_SPOT,
    SMT_OMNI
};

HYP_STRUCT()
struct ShadowMapAtlasElement
{
    // Directional and spot lights only: index of the atlas in the shadow map texture array
    uint32 atlasIndex = ~0u;

    // Point light shadow maps only: index of the cubemap in the texture array
    uint32 pointLightIndex = ~0u;

    // Index of the element in the atlas
    uint32 index = ~0u;

    // Offset in the atlas texture array, in uv space
    Vec2f offsetUv;

    // Offset in the atlas texture array, in pixels
    Vec2u offsetCoords;

    // Dimensions of the shadow map in pixels
    Vec2u dimensions;

    // Shadow map dimensions relative to the atlas dimensions
    Vec2f scale;

    HYP_FORCE_INLINE bool operator==(const ShadowMapAtlasElement& other) const
    {
        return atlasIndex == other.atlasIndex
            && pointLightIndex == other.pointLightIndex
            && index == other.index
            && offsetUv == other.offsetUv
            && offsetCoords == other.offsetCoords
            && dimensions == other.dimensions
            && scale == other.scale;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowMapAtlasElement& other) const
    {
        return atlasIndex != other.atlasIndex
            || pointLightIndex != other.pointLightIndex
            || index != other.index
            || offsetUv != other.offsetUv
            || offsetCoords != other.offsetCoords
            || dimensions != other.dimensions
            || scale != other.scale;
    }
};

HYP_STRUCT()
struct ShadowMapAtlas : AtlasPacker<ShadowMapAtlasElement>
{
    HYP_PROPERTY(AtlasDimensions, &ShadowMapAtlas::atlasDimensions)
    HYP_PROPERTY(Elements, &ShadowMapAtlas::elements)
    HYP_PROPERTY(FreeSpaces, &ShadowMapAtlas::freeSpaces)

    HYP_FIELD(Property = "AtlasIndex", Serialize = true)
    uint32 atlasIndex;

    ShadowMapAtlas()
        : AtlasPacker<ShadowMapAtlasElement>(Vec2u(0, 0)),
          atlasIndex(~0u)
    {
    }

    ShadowMapAtlas(uint32 atlasIndex, const Vec2u& atlasDimensions)
        : AtlasPacker(atlasDimensions),
          atlasIndex(atlasIndex)
    {
    }

    ShadowMapAtlas(const ShadowMapAtlas& other)
        : AtlasPacker(static_cast<const AtlasPacker<ShadowMapAtlasElement>&>(other)),
          atlasIndex(other.atlasIndex)
    {
    }

    ShadowMapAtlas(ShadowMapAtlas&& other) noexcept
        : AtlasPacker(std::move(static_cast<AtlasPacker<ShadowMapAtlasElement>&&>(other))),
          atlasIndex(other.atlasIndex)
    {
        other.atlasIndex = ~0u;
    }

    ShadowMapAtlas& operator=(const ShadowMapAtlas& other)
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(other);
        atlasIndex = other.atlasIndex;

        return *this;
    }

    ShadowMapAtlas& operator=(ShadowMapAtlas&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        AtlasPacker<ShadowMapAtlasElement>::operator=(std::move(other));

        atlasIndex = other.atlasIndex; // NOLINT(bugprone-use-after-move)
        other.atlasIndex = ~0u;

        return *this;
    }

    bool AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement& outElement);
};

class ShadowMapAllocator
{
public:
    ShadowMapAllocator();
    ~ShadowMapAllocator();

    HYP_FORCE_INLINE const ImageRef& GetAtlasImage() const
    {
        return m_atlasImage;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetAtlasImageView() const
    {
        return m_atlasImageView;
    }

    HYP_FORCE_INLINE const ImageRef& GetPointLightShadowMapImage() const
    {
        return m_pointLightShadowMapImage;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetPointLightShadowMapImageView() const
    {
        return m_pointLightShadowMapImageView;
    }

    void Initialize();
    void Destroy();

    RenderShadowMap* AllocateShadowMap(ShadowMapType shadowMapType, ShadowMapFilter filterMode, const Vec2u& dimensions);
    bool FreeShadowMap(RenderShadowMap* shadowMap);

private:
    Vec2u m_atlasDimensions;
    Array<ShadowMapAtlas> m_atlases;

    ImageRef m_atlasImage;
    ImageViewRef m_atlasImageView;

    ImageRef m_pointLightShadowMapImage;
    ImageViewRef m_pointLightShadowMapImageView;

    IdGenerator m_pointLightShadowMapIdGenerator;
};

class RenderShadowMap final : public RenderResourceBase
{
public:
    RenderShadowMap(ShadowMapType type, ShadowMapFilter filterMode, const ShadowMapAtlasElement& atlasElement, const ImageViewRef& imageView);
    RenderShadowMap(RenderShadowMap&& other) noexcept;
    virtual ~RenderShadowMap() override;

    HYP_FORCE_INLINE ShadowMapType GetShadowMapType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ShadowMapFilter GetFilterMode() const
    {
        return m_filterMode;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_atlasElement.dimensions;
    }

    HYP_FORCE_INLINE const ShadowMapAtlasElement& GetAtlasElement() const
    {
        return m_atlasElement;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void UpdateBufferData();

    ShadowMapType m_type;
    ShadowMapFilter m_filterMode;
    ShadowMapAtlasElement m_atlasElement;
    ImageViewRef m_imageView;

    Handle<FullScreenPass> m_combineShadowMapsPass;
};

struct HYP_API ShadowPassData : PassData
{
    virtual ~ShadowPassData() override;
};

struct ShadowPassDataExt : PassDataExt
{
    Light* light = nullptr;

    ShadowPassDataExt()
        : PassDataExt(TypeId::ForType<ShadowPassDataExt>())
    {
    }

    virtual ~ShadowPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        ShadowPassDataExt* clone = new ShadowPassDataExt;
        clone->light = light;

        return clone;
    }
};

class ShadowRendererBase : public RendererBase
{
public:
    virtual ~ShadowRendererBase() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override final;

protected:
    ShadowRendererBase();

    virtual int RunCleanupCycle(int maxIter) override;

    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

    virtual RenderShadowMap* AllocateShadowMap(Light* light) = 0;

private:
    // Shadow maps cached per-light.
    // Since Lights can have multiple shadow views that blit into one final shadow map
    // we store the shadow maps here rather than on the per-view PassData
    struct CachedShadowMapData
    {
        RenderShadowMap* shadowMap = nullptr;
        Handle<FullScreenPass> combineShadowMapsPass; // Pass to combine shadow maps for this light (optional)
        ImageRef combinedShadowMapsBlurred;
        ComputePipelineRef csBlurShadowMap; // compute pipeline for blurring VSM shadow maps
    };

    /// Cached per-light shadow map rendering data that is cleaned up when no longer used
    HashMap<WeakHandle<Light>, CachedShadowMapData> m_cachedShadowMapData;
};

class PointShadowRenderer : public ShadowRendererBase
{
public:
    PointShadowRenderer() = default;
    virtual ~PointShadowRenderer() override = default;

protected:
    virtual RenderShadowMap* AllocateShadowMap(Light* light) override;
};

class DirectionalShadowRenderer : public ShadowRendererBase
{
public:
    DirectionalShadowRenderer() = default;
    virtual ~DirectionalShadowRenderer() override = default;

protected:
    virtual RenderShadowMap* AllocateShadowMap(Light* light) override;
};

} // namespace hyperion
