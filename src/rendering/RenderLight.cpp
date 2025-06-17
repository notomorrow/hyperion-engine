/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <scene/Material.hpp>
#include <scene/Light.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region RenderLight

RenderLight::RenderLight(Light* light)
    : m_light(light),
      m_bufferData {}
{
}

RenderLight::RenderLight(RenderLight&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_light(other.m_light),
      m_material(std::move(other.m_material)),
      m_renderMaterial(std::move(other.m_renderMaterial)),
      m_shadowMap(std::move(other.m_shadowMap)),
      m_bufferData(std::move(other.m_bufferData))
{
    other.m_light = nullptr;
}

RenderLight::~RenderLight() = default;

void RenderLight::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderLight::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderLight::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GpuBufferHolderBase* RenderLight::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_LIGHTS];
}

void RenderLight::UpdateBufferData()
{
    HYP_SCOPE;

    /*LightShaderData* bufferData = static_cast<LightShaderData*>(m_bufferAddress);

    *bufferData = m_bufferData;

    // override material buffer index
    bufferData->materialIndex = m_renderMaterial
        ? m_renderMaterial->GetBufferIndex()
        : ~0u;

    if (m_shadowMap)
    {
        bufferData->shadowMapIndex = m_shadowMap->GetBufferIndex();
    }
    else
    {
        bufferData->shadowMapIndex = ~0u;
    }

    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);*/
}

void RenderLight::SetMaterial(const Handle<Material>& material)
{
    HYP_SCOPE;

    Execute([this, material]()
        {
            m_material = material;

            if (m_material.IsValid())
            {
                m_renderMaterial = TResourceHandle<RenderMaterial>(m_material->GetRenderResource());
            }
            else
            {
                m_renderMaterial = TResourceHandle<RenderMaterial>();
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderLight::SetBufferData(const LightShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            m_bufferData = bufferData;

            if (m_shadowMap)
            {
                m_bufferData.shadowMapIndex = m_shadowMap->GetBufferIndex();
            }
            else
            {
                m_bufferData.shadowMapIndex = ~0u;
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderLight::SetShadowMap(TResourceHandle<RenderShadowMap>&& shadowMap)
{
    HYP_SCOPE;

    Execute([this, shadowMap = std::move(shadowMap)]()
        {
            m_shadowMap = std::move(shadowMap);

            if (m_shadowMap)
            {
                m_bufferData.shadowMapIndex = m_shadowMap->GetBufferIndex();
            }
            else
            {
                m_bufferData.shadowMapIndex = ~0u;
            }

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

#pragma endregion RenderLight

HYP_DESCRIPTOR_SSBO(Global, CurrentLight, 1, sizeof(LightShaderData), true);
HYP_DESCRIPTOR_SSBO(Global, LightsBuffer, 1, sizeof(LightShaderData), false);

} // namespace hyperion
