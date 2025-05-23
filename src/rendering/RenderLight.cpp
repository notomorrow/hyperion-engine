/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <scene/Material.hpp>
#include <scene/Light.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region LightRenderResource

LightRenderResource::LightRenderResource(Light *light)
    : m_light(light),
      m_buffer_data { }
{
}

LightRenderResource::LightRenderResource(LightRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_light(other.m_light),
      m_material(std::move(other.m_material)),
      m_material_render_resource_handle(std::move(other.m_material_render_resource_handle)),
      m_shadow_map_render_resource_handle(std::move(other.m_shadow_map_render_resource_handle)),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_light = nullptr;
}

LightRenderResource::~LightRenderResource() = default;

void LightRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
    
    UpdateBufferData();
}

void LightRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void LightRenderResource::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GPUBufferHolderBase *LightRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->lights;
}

void LightRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    LightShaderData *buffer_data = static_cast<LightShaderData *>(m_buffer_address);

    *buffer_data = m_buffer_data;

    // override material buffer index
    buffer_data->material_index = m_material_render_resource_handle
        ? m_material_render_resource_handle->GetBufferIndex()
        : ~0u;

    if (m_shadow_map_render_resource_handle) {
        buffer_data->shadow_map_index = m_shadow_map_render_resource_handle->GetBufferIndex();
    } else {
        buffer_data->shadow_map_index = ~0u;
    }

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void LightRenderResource::SetMaterial(const Handle<Material> &material)
{
    HYP_SCOPE;

    Execute([this, material]()
    {
        m_material = material;

        if (m_material.IsValid()) {
            m_material_render_resource_handle = TResourceHandle<MaterialRenderResource>(m_material->GetRenderResource());
        } else {
            m_material_render_resource_handle = TResourceHandle<MaterialRenderResource>();
        }

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

void LightRenderResource::SetBufferData(const LightShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (m_shadow_map_render_resource_handle) {
            m_buffer_data.shadow_map_index = m_shadow_map_render_resource_handle->GetBufferIndex();
        } else {
            m_buffer_data.shadow_map_index = ~0u;
        }

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

void LightRenderResource::SetShadowMapResourceHandle(TResourceHandle<ShadowMapRenderResource> &&shadow_map_render_resource_handle)
{
    HYP_SCOPE;

    Execute([this, shadow_map_render_resource_handle = std::move(shadow_map_render_resource_handle)]()
    {
        m_shadow_map_render_resource_handle = std::move(shadow_map_render_resource_handle);

        if (m_shadow_map_render_resource_handle) {
            m_buffer_data.shadow_map_index = m_shadow_map_render_resource_handle->GetBufferIndex();
        } else {
            m_buffer_data.shadow_map_index = ~0u;
        }

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

#pragma endregion LightRenderResource

namespace renderer {

HYP_DESCRIPTOR_SSBO(Global, CurrentLight, 1, sizeof(LightShaderData), true);
HYP_DESCRIPTOR_SSBO(Global, LightsBuffer, 1, sizeof(LightShaderData), false);

} // namespace renderer

} // namespace hyperion
