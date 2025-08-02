/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderImageView.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanImage;

class VulkanImageView final : public ImageViewBase
{
public:
    VulkanImageView(
        const VulkanImageRef& image);

    VulkanImageView(
        const VulkanImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 faceIndex,
        uint32 numFaces);

    virtual ~VulkanImageView() override;

    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

private:
    VkImageView m_handle;
};

} // namespace hyperion
