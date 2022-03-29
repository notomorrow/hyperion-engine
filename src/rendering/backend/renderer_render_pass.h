#ifndef HYPERION_RENDERER_RENDER_PASS_H
#define HYPERION_RENDERER_RENDER_PASS_H

#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_attachment.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer;

class RenderPass {
    friend class FramebufferObject;
    friend class GraphicsPipeline;
public:
    enum Stage {
        RENDER_PASS_STAGE_NONE = 0,
        RENDER_PASS_STAGE_PRESENT = 1, /* for presentation on screen */
        RENDER_PASS_STAGE_SHADER = 2,  /* for use as a sampled texture in a shader */
        RENDER_PASS_STAGE_BLIT = 3     /* for blitting into another framebuffer in a later stage */
    };

    enum Mode {
        RENDER_PASS_INLINE = 0,
        RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
    };

    struct Attachment {
        Image::InternalFormat format;
    };

    RenderPass(Stage stage, Mode mode);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    inline Stage GetStage() const
        { return m_stage; }

    /* Pre-defined attachments */

    inline void AddAttachment(const Attachment &attachment)
    {
        m_attachments.push_back(std::make_pair(uint32_t(m_color_attachments.size() + m_depth_attachments.size() + m_attachments.size()), attachment));
        std::sort(m_attachments.begin(), m_attachments.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
    }

    inline auto &GetAttachments() { return m_attachments; }
    inline const auto &GetAttachments() const { return m_attachments; }

    /* Renderer attachments - the defined attachments get added this way,
     * as well as any custom attachments */

    void AddDepthAttachment(std::unique_ptr<AttachmentBase> &&);
    void AddColorAttachment(std::unique_ptr<AttachmentBase> &&);

    inline auto &GetColorAttachments() { return m_color_attachments; }
    inline const auto &GetColorAttachments() const { return m_color_attachments; }
    inline auto &GetDepthAttachments() { return m_depth_attachments; }
    inline const auto &GetDepthAttachments() const { return m_depth_attachments; }

    inline VkRenderPass GetRenderPass() const { return m_render_pass; }

    Result Create(Device *device);
    Result Destroy(Device *device);

    void Begin(CommandBuffer *cmd,
        VkFramebuffer framebuffer,
        VkExtent2D extent,
        VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);
    void End(CommandBuffer *cmd);

private:
    void CreateAttachments();
    void CreateDependencies();

    inline void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    Stage m_stage;
    Mode m_mode;
    std::vector<std::pair<uint32_t, Attachment>> m_attachments;

    std::vector<std::unique_ptr<AttachmentBase>> m_color_attachments;
    std::vector<std::unique_ptr<AttachmentBase>> m_depth_attachments;

    std::vector<VkSubpassDependency> m_dependencies;
    std::vector<VkClearValue> m_clear_values;

    VkRenderPass m_render_pass;
};

} // namespace renderer
} // namespace hyperion

#endif