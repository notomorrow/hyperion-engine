#include "renderer_attachment.h"

namespace hyperion {

RendererAttachment::RendererAttachment(VkFormat image_format,
    VkAttachmentLoadOp load_op,
    VkAttachmentStoreOp store_op,
    VkAttachmentLoadOp stencil_load_op,
    VkAttachmentStoreOp stencil_store_op,
    VkImageLayout final_layout,
    uint32_t ref_attachment,
    VkImageLayout ref_layout)
    : m_image_format(image_format),
      m_load_op(load_op),
      m_store_op(store_op),
      m_stencil_load_op(stencil_load_op),
      m_stencil_store_op(stencil_store_op),
      m_final_layout(final_layout),
      m_ref_attachment(ref_attachment),
      m_ref_layout(ref_layout),
      m_attachment_reference{}
{
}

RendererAttachment::~RendererAttachment()
{

}


RendererResult RendererAttachment::Create(RendererDevice *device)
{
    VkAttachmentDescription attachment{};
    attachment.format = m_image_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    attachment.loadOp = m_load_op;
    attachment.storeOp = m_store_op;
    attachment.stencilLoadOp = m_stencil_load_op;
    attachment.stencilStoreOp = m_stencil_store_op;
    attachment.finalLayout = m_final_layout;

    m_attachment_reference.attachment = m_ref_attachment;
    m_attachment_reference.layout = m_ref_layout;

    HYPERION_RETURN_OK;
}


RendererResult RendererAttachment::Destroy(RendererDevice *device)
{
    HYPERION_RETURN_OK;
}


} // namespace hyperion