#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <Types.hpp>

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
class FramebufferObject<Platform::VULKAN>
{
public:
    FramebufferObject(Extent2D extent);
    FramebufferObject(Extent3D extent);
    FramebufferObject(const FramebufferObject &other) = delete;
    FramebufferObject &operator=(const FramebufferObject &other) = delete;
    ~FramebufferObject();

    VkFramebuffer GetHandle() const { return m_handle; }

    Result Create(Device<Platform::VULKAN> *device, RenderPass<Platform::VULKAN> *render_pass);
    Result Destroy(Device<Platform::VULKAN> *device);

    void AddAttachmentUsage(AttachmentUsage *attachment_usage);
    bool RemoveAttachmentUsage(const Attachment *attachment);

    auto &GetAttachmentUsages() { return m_attachment_usages; }
    const auto &GetAttachmentUsages() const { return m_attachment_usages; }

    uint GetWidth() const { return m_extent.width; }
    uint GetHeight() const { return m_extent.height; }

private:
    Extent3D m_extent;

    std::vector<AttachmentUsage *> m_attachment_usages;

    VkFramebuffer m_handle;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
