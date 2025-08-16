/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderSampler.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanSampler final : public SamplerBase
{
    HYP_OBJECT_BODY(VulkanSampler);

public:
    VulkanSampler(
        TextureFilterMode minFilterMode = TFM_NEAREST,
        TextureFilterMode magFilterMode = TFM_NEAREST,
        TextureWrapMode wrapMode = TWM_CLAMP_TO_EDGE);

    virtual ~VulkanSampler() override;

    HYP_FORCE_INLINE VkSampler GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkSampler m_handle;
};

} // namespace hyperion
