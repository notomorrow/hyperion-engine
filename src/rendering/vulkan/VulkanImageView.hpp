/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderImageView.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanImage;

class VulkanImageView final : public ImageViewBase
{
public:
    HYP_API VulkanImageView(
        const VulkanImageRef& image);

    HYP_API VulkanImageView(
        const VulkanImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 faceIndex,
        uint32 numFaces);

    HYP_API virtual ~VulkanImageView() override;

    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    VkImageView m_handle;
};

} // namespace hyperion
