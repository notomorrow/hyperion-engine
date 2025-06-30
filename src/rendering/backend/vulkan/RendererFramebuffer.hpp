/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP

#include <rendering/backend/RendererFramebuffer.hpp>

#include <rendering/backend/vulkan/RendererAttachment.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererRenderPass.hpp>

#include <core/containers/FlatMap.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanCommandBuffer;

struct VulkanAttachmentDef
{
    VulkanImageRef image;
    VulkanAttachmentRef attachment;
};

struct VulkanAttachmentMap
{
    using Iterator = typename FlatMap<uint32, VulkanAttachmentDef>::Iterator;
    using ConstIterator = typename FlatMap<uint32, VulkanAttachmentDef>::ConstIterator;

    VulkanFramebufferWeakRef framebufferWeak;
    FlatMap<uint32, VulkanAttachmentDef> attachments;

    ~VulkanAttachmentMap()
    {
        Reset();
    }

    RendererResult Create();
    RendererResult Resize(Vec2u newSize);

    void Reset()
    {
        for (auto& it : attachments)
        {
            SafeRelease(std::move(it.second.attachment));
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return attachments.Size();
    }

    HYP_FORCE_INLINE const VulkanAttachmentRef& GetAttachment(uint32 binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End())
        {
            return VulkanAttachmentRef::Null();
        }

        return it->second.attachment;
    }

    HYP_FORCE_INLINE VulkanAttachmentRef AddAttachment(const VulkanAttachmentRef& attachment)
    {
        AssertThrow(attachment.IsValid());
        AssertThrow(attachment->GetImage().IsValid());

        AssertThrowMsg(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        AssertThrowMsg(!attachments.Contains(binding), "Attachment already exists at binding: %u", binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                VulkanImageRef(attachment->GetImage()),
                attachment });

        return attachment;
    }

    HYP_FORCE_INLINE VulkanAttachmentRef AddAttachment(
        uint32 binding,
        Vec2u extent,
        TextureFormat format,
        TextureType type,
        RenderPassStage stage,
        LoadOperation loadOp,
        StoreOperation storeOp)
    {
        TextureDesc textureDesc;
        textureDesc.type = type;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        VulkanImageRef image = MakeRenderObject<VulkanImage>(textureDesc);

        VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(image, framebufferWeak, stage, loadOp, storeOp);
        attachment->SetBinding(binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                image,
                attachment });

        return attachment;
    }

    HYP_DEF_STL_BEGIN_END(attachments.Begin(), attachments.End())
};

class VulkanFramebuffer final : public FramebufferBase
{
public:
    HYP_API VulkanFramebuffer(
        Vec2u extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        uint32 numMultiviewLayers = 0);

    HYP_API virtual ~VulkanFramebuffer() override;

    HYP_FORCE_INLINE const FixedArray<VkFramebuffer, maxFramesInFlight>& GetVulkanHandles() const
    {
        return m_handles;
    }

    HYP_FORCE_INLINE const VulkanRenderPassRef& GetRenderPass() const
    {
        return m_renderPass;
    }

    HYP_API virtual AttachmentRef AddAttachment(const AttachmentRef& attachment) override;
    HYP_API virtual AttachmentRef AddAttachment(uint32 binding, const ImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;

    HYP_API virtual AttachmentRef AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

    HYP_API virtual bool RemoveAttachment(uint32 binding) override;

    HYP_API virtual AttachmentBase* GetAttachment(uint32 binding) const override;

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult Resize(Vec2u newSize) override;

    HYP_API virtual void BeginCapture(CommandBufferBase* commandBuffer, uint32 frameIndex) override;
    HYP_API virtual void EndCapture(CommandBufferBase* commandBuffer, uint32 frameIndex) override;

private:
    FixedArray<VkFramebuffer, maxFramesInFlight> m_handles;
    VulkanRenderPassRef m_renderPass;
    VulkanAttachmentMap m_attachmentMap;
};

} // namespace hyperion

#endif // RENDERER_FBO_HPP
