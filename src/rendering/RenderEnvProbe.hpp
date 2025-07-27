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
class Texture;
class RenderCamera;
class RenderShadowMap;

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
