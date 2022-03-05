#ifndef HYPERION_RENDERER_ATTACHMENT_H
#define HYPERION_RENDERER_ATTACHMENT_H

#include "renderer_device.h"

#include <vulkan/vulkan.h>

namespace hyperion {

class RendererAttachment {
public:
    RendererAttachment(VkFormat image_format,
        VkAttachmentLoadOp load_op,
        VkAttachmentStoreOp store_op,
        VkAttachmentLoadOp stencil_load_op,
        VkAttachmentStoreOp stencil_store_op,
        VkImageLayout final_layout,
        uint32_t ref_attachment,
        VkImageLayout ref_layout
    );
    RendererAttachment(const RendererAttachment &other) = delete;
    RendererAttachment &operator=(const RendererAttachment &other) = delete;
    ~RendererAttachment();

    RendererResult Create(RendererDevice *device);
    RendererResult Destroy(RendererDevice *device);

private:
    VkFormat m_image_format;
    VkAttachmentLoadOp m_load_op;
    VkAttachmentStoreOp m_store_op;
    VkAttachmentLoadOp m_stencil_load_op;
    VkAttachmentStoreOp m_stencil_store_op;
    VkImageLayout m_final_layout;
    uint32_t m_ref_attachment;
    VkImageLayout m_ref_layout;
    VkAttachmentReference m_attachment_reference;
};

} // namespace hyperion

#endif