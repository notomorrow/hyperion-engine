#ifndef HYPERION_RENDERER_RENDER_PASS_H
#define HYPERION_RENDERER_RENDER_PASS_H

#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_attachment.h"

#include <vulkan/vulkan.h>

#include <optional>

namespace hyperion {
namespace renderer {

class CommandBuffer;
class FramebufferObject;

class RenderPass {
    friend class FramebufferObject;
    friend class GraphicsPipeline;
public:

    enum class Mode {
        RENDER_PASS_INLINE = 0,
        RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
    };

    RenderPass(RenderPassStage stage, Mode mode);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    inline RenderPassStage GetStage() const
        { return m_stage; }

    void AddAttachmentRef(AttachmentRef *attachment_ref)
    {
        attachment_ref->IncRef();

        m_render_pass_attachment_refs.push_back(attachment_ref);
    }

    inline auto &GetAttachmentRefs()             { return m_render_pass_attachment_refs; }
    inline const auto &GetAttachmentRefs() const { return m_render_pass_attachment_refs; }

    inline VkRenderPass GetRenderPass() const { return m_render_pass; }

    Result Create(Device *device);
    Result Destroy(Device *device);

    void Begin(CommandBuffer *cmd, FramebufferObject *framebuffer);
    void End(CommandBuffer *cmd);

private:
    void CreateDependencies();

    inline void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    RenderPassStage m_stage;
    Mode m_mode;

    std::vector<AttachmentRef *> m_render_pass_attachment_refs;

    std::vector<VkSubpassDependency> m_dependencies;
    std::vector<VkClearValue> m_clear_values;

    VkRenderPass m_render_pass;
};

} // namespace renderer
} // namespace hyperion

#endif