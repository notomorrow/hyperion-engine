#include "renderer_attachment.h"

namespace hyperion {
namespace renderer {
Attachment::Attachment(VkFormat format,
    VkAttachmentLoadOp load_op,
    VkAttachmentStoreOp store_op,
    VkAttachmentLoadOp stencil_load_op,
    VkAttachmentStoreOp stencil_store_op,
    VkImageLayout final_layout,
    uint32_t ref_attachment,
    VkImageLayout ref_layout)
    : m_format(format),
      m_load_op(load_op),
      m_store_op(store_op),
      m_stencil_load_op(stencil_load_op),
      m_stencil_store_op(stencil_store_op),
      m_final_layout(final_layout),
      m_ref_attachment(ref_attachment),
      m_ref_layout(ref_layout),
      m_attachment_reference{},
      m_attachment_description{}
{
}

Attachment::~Attachment()
{

}

Result Attachment::Create(Device *device)
{
    m_attachment_description.format = m_format;
    m_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    m_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_attachment_description.loadOp = m_load_op;
    m_attachment_description.storeOp = m_store_op;
    m_attachment_description.stencilLoadOp = m_stencil_load_op;
    m_attachment_description.stencilStoreOp = m_stencil_store_op;
    m_attachment_description.finalLayout = m_final_layout;

    m_attachment_reference.attachment = m_ref_attachment;
    m_attachment_reference.layout = m_ref_layout;

    HYPERION_RETURN_OK;
}


Result Attachment::Destroy(Device *device)
{
    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion