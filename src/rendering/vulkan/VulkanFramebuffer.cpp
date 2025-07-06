/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region VulkanAttachmentMap

RendererResult VulkanAttachmentMap::Create()
{
    VulkanFramebufferRef framebuffer = framebufferWeak.Lock();
    if (!framebuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Framebuffer is not valid");
    }

    Array<VulkanImageRef> imagesToTransition;

    for (KeyValuePair<uint32, VulkanAttachmentDef>& it : attachments)
    {
        VulkanAttachmentDef& def = it.second;

        HYP_GFX_ASSERT(def.image.IsValid());

        if (!def.image->IsCreated())
        {
            HYPERION_BUBBLE_ERRORS(def.image->Create());
        }

        imagesToTransition.PushBack(def.image);

        HYP_GFX_ASSERT(def.attachment.IsValid());

        if (!def.attachment->IsCreated())
        {
            HYPERION_BUBBLE_ERRORS(def.attachment->Create());
        }
    }

    // Transition image layout immediately on creation
    if (imagesToTransition.Any())
    {
        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

        singleTimeCommands->Push([&](CmdList& cmd) -> RendererResult
            {
                for (const VulkanImageRef& image : imagesToTransition)
                {
                    HYP_GFX_ASSERT(image.IsValid());

                    if (framebuffer->GetRenderPass()->GetStage() == RenderPassStage::PRESENT)
                    {
                        cmd.Add<InsertBarrier>(image, RS_PRESENT);
                    }
                    else
                    {
                        cmd.Add<InsertBarrier>(image, RS_SHADER_RESOURCE);
                    }
                }

                return {};
            });

        HYPERION_BUBBLE_ERRORS(singleTimeCommands->Execute());
    }

    return {};
}

RendererResult VulkanAttachmentMap::Resize(Vec2u newSize)
{
    VulkanFramebufferRef framebuffer = framebufferWeak.Lock();
    if (!framebuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Framebuffer is not valid");
    }

    Array<VulkanImageRef> imagesToTransition;

    for (KeyValuePair<uint32, VulkanAttachmentDef>& it : attachments)
    {
        VulkanAttachmentDef& def = it.second;

        HYP_GFX_ASSERT(def.image.IsValid());

        VulkanImageRef newImage = def.image;

        if (def.attachment->GetFramebuffer() == framebufferWeak)
        {
            TextureDesc textureDesc = def.image->GetTextureDesc();
            textureDesc.extent = Vec3u { newSize.x, newSize.y, 1 };

            newImage = MakeRenderObject<VulkanImage>(textureDesc);
            HYPERION_ASSERT_RESULT(newImage->Create());

            imagesToTransition.PushBack(newImage);

            if (def.image.IsValid())
            {
                SafeRelease(std::move(def.image));
            }
        }
        else
        {
            if (def.image->GetExtent().GetXY() != newSize)
            {
                return HYP_MAKE_ERROR(RendererError, "Expected image to have a size matching {} but got size: {}", 0,
                    newSize, def.image->GetExtent().GetXY());
            }
        }

        VulkanAttachmentRef newAttachment = MakeRenderObject<VulkanAttachment>(
            newImage,
            framebufferWeak,
            def.attachment->GetRenderPassStage(),
            def.attachment->GetLoadOperation(),
            def.attachment->GetStoreOperation());

        newAttachment->SetBinding(def.attachment->GetBinding());

        HYPERION_ASSERT_RESULT(newAttachment->Create());

        if (def.attachment.IsValid())
        {
            SafeRelease(std::move(def.attachment));
        }

        def = VulkanAttachmentDef {
            std::move(newImage),
            std::move(newAttachment)
        };
    }

    // Transition image layout immediately on resize
    if (imagesToTransition.Any())
    {
        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

        singleTimeCommands->Push([this, &framebuffer, &imagesToTransition](CmdList& cmd)
            {
                for (const VulkanImageRef& image : imagesToTransition)
                {
                    HYP_GFX_ASSERT(image.IsValid());

                    if (framebuffer->GetRenderPass()->GetStage() == RenderPassStage::PRESENT)
                    {
                        cmd.Add<InsertBarrier>(image, RS_PRESENT);
                    }
                    else
                    {
                        cmd.Add<InsertBarrier>(image, RS_SHADER_RESOURCE);
                    }
                }
            });

        HYPERION_ASSERT_RESULT(singleTimeCommands->Execute());
    }

    return {};
}

#pragma endregion VulkanAttachmentMap

#pragma region VulkanFramebuffer

VulkanFramebuffer::VulkanFramebuffer(
    Vec2u extent,
    RenderPassStage stage,
    uint32 numMultiviewLayers)
    : FramebufferBase(extent),
      m_handles { VK_NULL_HANDLE },
      m_renderPass(MakeRenderObject<VulkanRenderPass>(stage, RenderPassMode::RENDER_PASS_INLINE, numMultiviewLayers))
{
    m_attachmentMap.framebufferWeak = VulkanFramebufferWeakRef(WeakHandleFromThis());
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        HYP_GFX_ASSERT(m_handles[frameIndex] == VK_NULL_HANDLE, "Expected framebuffer to have been destroyed");
    }
}

bool VulkanFramebuffer::IsCreated() const
{
    return m_handles[0] != VK_NULL_HANDLE;
}

RendererResult VulkanFramebuffer::Create()
{
    if (IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachmentMap.Create());

    for (const auto& it : m_attachmentMap.attachments)
    {
        const VulkanAttachmentDef& def = it.second;

        HYP_GFX_ASSERT(def.attachment.IsValid());
        m_renderPass->AddAttachment(def.attachment);
    }

    HYPERION_BUBBLE_ERRORS(m_renderPass->Create());

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;

    for (const auto& it : m_attachmentMap.attachments)
    {
        HYP_GFX_ASSERT(it.second.attachment != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView() != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(static_cast<VulkanImageView*>(it.second.attachment->GetImageView().Get())->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass->GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = m_extent.x;
    framebufferCreateInfo.height = m_extent.y;
    framebufferCreateInfo.layers = numLayers;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        HYPERION_VK_CHECK(vkCreateFramebuffer(
            GetRenderBackend()->GetDevice()->GetDevice(),
            &framebufferCreateInfo,
            nullptr,
            &m_handles[frameIndex]));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanFramebuffer::Destroy()
{
    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        if (m_handles[frameIndex] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), m_handles[frameIndex], nullptr);
            m_handles[frameIndex] = VK_NULL_HANDLE;
        }
    }

    SafeRelease(std::move(m_renderPass));

    m_attachmentMap.Reset();

    HYPERION_RETURN_OK;
}

RendererResult VulkanFramebuffer::Resize(Vec2u newSize)
{
    if (m_extent == newSize)
    {
        HYPERION_RETURN_OK;
    }

    m_extent = newSize;

    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachmentMap.Resize(newSize));

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        if (m_handles[frameIndex] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), m_handles[frameIndex], nullptr);
            m_handles[frameIndex] = VK_NULL_HANDLE;
        }
    }

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;

    for (const auto& it : m_attachmentMap.attachments)
    {
        HYP_GFX_ASSERT(it.second.attachment != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView() != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(static_cast<VulkanImageView*>(it.second.attachment->GetImageView().Get())->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass->GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = newSize.x;
    framebufferCreateInfo.height = newSize.y;
    framebufferCreateInfo.layers = numLayers;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        HYPERION_VK_CHECK(vkCreateFramebuffer(
            GetRenderBackend()->GetDevice()->GetDevice(),
            &framebufferCreateInfo,
            nullptr,
            &m_handles[frameIndex]));
    }

    HYPERION_RETURN_OK;
}

AttachmentRef VulkanFramebuffer::AddAttachment(const AttachmentRef& attachment)
{
    HYP_GFX_ASSERT(attachment->GetFramebuffer() == this,
        "Attachment framebuffer does not match framebuffer");

    return m_attachmentMap.AddAttachment(VulkanAttachmentRef(attachment));
}

AttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const ImageRef& image,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(
        VulkanImageRef(image),
        VulkanFramebufferWeakRef(WeakHandleFromThis()),
        m_renderPass->GetStage(),
        loadOp,
        storeOp);

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

AttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    TextureFormat format,
    TextureType type,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    return m_attachmentMap.AddAttachment(
        binding,
        m_extent,
        format,
        type,
        m_renderPass->GetStage(),
        loadOp,
        storeOp);
}

bool VulkanFramebuffer::RemoveAttachment(uint32 binding)
{
    const auto it = m_attachmentMap.attachments.Find(binding);

    if (it == m_attachmentMap.attachments.End())
    {
        return false;
    }

    SafeRelease(std::move(it->second.attachment));

    m_attachmentMap.attachments.Erase(it);

    return true;
}

AttachmentBase* VulkanFramebuffer::GetAttachment(uint32 binding) const
{
    return m_attachmentMap.GetAttachment(binding).Get();
}

void VulkanFramebuffer::BeginCapture(CommandBufferBase* commandBuffer, uint32 frameIndex)
{
    m_renderPass->Begin(static_cast<VulkanCommandBuffer*>(commandBuffer), this, frameIndex);
}

void VulkanFramebuffer::EndCapture(CommandBufferBase* commandBuffer, uint32 frameIndex)
{
    m_renderPass->End(static_cast<VulkanCommandBuffer*>(commandBuffer));
}

#pragma endregion VulkanFramebuffer

} // namespace hyperion
