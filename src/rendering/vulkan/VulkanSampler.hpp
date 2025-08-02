/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderSampler.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanSampler final : public SamplerBase
{
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
    virtual RendererResult Destroy() override;

private:
    VkSampler m_handle;
};

} // namespace hyperion
