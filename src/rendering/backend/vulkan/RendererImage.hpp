/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_BACKEND_RENDERER_VULKAN_IMAGE_HPP
#define RENDERER_BACKEND_RENDERER_VULKAN_IMAGE_HPP

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <core/math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
class VulkanImage final : public ImageBase
{
public:
    friend class VulkanSwapchain;

    HYP_API VulkanImage(const TextureDesc& textureDesc);
    HYP_API virtual ~VulkanImage() override;

    HYP_FORCE_INLINE VkImage GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_API virtual bool IsCreated() const override;
    HYP_API virtual bool IsOwned() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Create(ResourceState initialState) override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult Resize(const Vec3u& extent) override;

    HYP_API void SetResourceState(ResourceState newState);

    HYP_API ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    HYP_API void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    HYP_API virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    HYP_API virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const ImageBase* src) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const ImageBase* src,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const ImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const ImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    HYP_API virtual RendererResult GenerateMipmaps(CommandBufferBase* commandBuffer) override;

    HYP_API virtual void CopyFromBuffer(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer) const override;

    HYP_API virtual void CopyToBuffer(
        CommandBufferBase* commandBuffer,
        GpuBufferBase* dstBuffer) const override;

    /*! \brief Creates a view of the image for the specified array layer
     */
    HYP_API virtual ImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

    HYP_FORCE_INLINE uint8 GetBPP() const
    {
        return m_bpp;
    }

private:
    VkImage m_handle = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;

    VkImageTiling m_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags m_usageFlags = 0;

    HashMap<uint64, ResourceState> m_subResourceStates;

    // true if we created the VkImage, false otherwise (e.g retrieved from swapchain)
    bool m_isHandleOwned = true;

    SizeType m_size;
    SizeType m_bpp; // bytes per pixel
};

} // namespace hyperion

#endif
