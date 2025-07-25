/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvProbe;
class RenderEnvProbe;
class Texture;
class RenderCamera;
class RenderShadowMap;

class RenderEnvProbe final : public RenderResourceBase
{
public:
    friend class EnvProbeRenderer;
    friend class ReflectionProbeRenderer;

    RenderEnvProbe(EnvProbe* envProbe);
    virtual ~RenderEnvProbe() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE EnvProbe* GetEnvProbe() const
    {
        return m_envProbe;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 GetTextureSlot() const
    {
        return m_textureSlot;
    }

    void SetTextureSlot(uint32 textureSlot);

    HYP_FORCE_INLINE const Vec4i& GetPositionInGrid() const
    {
        return m_positionInGrid;
    }

    void SetPositionInGrid(const Vec4i& positionInGrid);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const EnvProbeShaderData& GetBufferData() const
    {
        return m_bufferData;
    }

    void SetBufferData(const EnvProbeShaderData& bufferData);

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMapRenderResourceHandle() const
    {
        return m_shadowMap;
    }

    void SetShadowMap(TResourceHandle<RenderShadowMap>&& shadowMap);

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE const EnvProbeSphericalHarmonics& GetSphericalHarmonics() const
    {
        return m_sphericalHarmonics;
    }

    void SetSphericalHarmonics(const EnvProbeSphericalHarmonics& sphericalHarmonics);

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void CreateShader();

    void UpdateBufferData();

    EnvProbe* m_envProbe;

    EnvProbeShaderData m_bufferData;

    uint32 m_textureSlot;

    Vec4i m_positionInGrid;

    ShaderRef m_shader;

    // temp
    EnvProbeSphericalHarmonics m_sphericalHarmonics;

    TResourceHandle<RenderShadowMap> m_shadowMap;
};

struct EnvProbePassData : PassData
{
    // for sky
    Vec4f cachedLightDirIntensity;
};

struct EnvProbePassDataExt : PassDataExt
{
    EnvProbe* envProbe = nullptr;

    EnvProbePassDataExt()
        : PassDataExt(TypeId::ForType<EnvProbePassDataExt>())
    {
    }

    virtual ~EnvProbePassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvProbePassDataExt* clone = new EnvProbePassDataExt;
        clone->envProbe = envProbe;

        return clone;
    }
};

class EnvProbeRenderer : public RendererBase
{
public:
    virtual ~EnvProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override final;

protected:
    EnvProbeRenderer();

    virtual void RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) = 0;

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
};

class ReflectionProbeRenderer : public EnvProbeRenderer
{
public:
    ReflectionProbeRenderer();
    virtual ~ReflectionProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

protected:
    void CreateShader();

    virtual void RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) override;

    void ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
    void ComputeSH(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);

    ShaderRef m_shader;
};

} // namespace hyperion
