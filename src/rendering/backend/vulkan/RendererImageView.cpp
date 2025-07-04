/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererImageView.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region VulkanImageView

VulkanImageView::VulkanImageView(const VulkanImageRef& image)
    : ImageViewBase(image),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanImageView::VulkanImageView(
    const VulkanImageRef& image,
    uint32 mipIndex,
    uint32 numMips,
    uint32 faceIndex,
    uint32 numFaces)
    : ImageViewBase(image, mipIndex, numMips, faceIndex, numFaces),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanImageView::~VulkanImageView()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE, "image view should have been destroyed");

    SafeRelease(std::move(m_image));
}

bool VulkanImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanImageView::Create()
{
    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create image view on uninitialized image");
    }

    if (m_faceIndex >= m_image->NumFaces())
    {
        return HYP_MAKE_ERROR(RendererError, "Face index out of bounds");
    }

    if (m_mipIndex >= m_image->NumMipmaps())
    {
        return HYP_MAKE_ERROR(RendererError, "Mip index out of bounds");
    }

    HYP_GFX_ASSERT(static_cast<const VulkanImage*>(m_image.Get())->GetVulkanHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo viewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = static_cast<const VulkanImage*>(m_image.Get())->GetVulkanHandle();
    viewInfo.viewType = helpers::ToVkImageViewType(m_image->GetType());
    viewInfo.format = helpers::ToVkFormat(m_image->GetTextureFormat());

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = helpers::ToVkImageAspect(m_image->GetTextureFormat());
    viewInfo.subresourceRange.baseMipLevel = m_mipIndex;
    viewInfo.subresourceRange.levelCount = m_numMips != 0 ? m_numMips : m_image->NumMipmaps();
    viewInfo.subresourceRange.baseArrayLayer = m_faceIndex;
    viewInfo.subresourceRange.layerCount = m_numFaces != 0 ? m_numFaces : m_image->NumFaces();

    // HYP_GFX_ASSERT(mipmapLayer < numMipmaps, "mipmap layer out of bounds");
    // HYP_GFX_ASSERT(faceLayer < m_numFaces, "face layer out of bounds");

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(GetRenderBackend()->GetDevice()->GetDevice(), &viewInfo, nullptr, &m_handle),
        "Failed to create image view");

    HYPERION_RETURN_OK;
}

RendererResult VulkanImageView::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);

        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

#pragma endregion VulkanImageView

} // namespace hyperion