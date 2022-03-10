#ifndef HYPERION_V2_RENDER_PASS_H
#define HYPERION_V2_RENDER_PASS_H

#include "base.h"

#include <rendering/backend/renderer_render_pass.h>

#include <memory>

namespace hyperion::v2 {

class RenderPass : public BaseComponent<renderer::RenderPass> {
public:
    enum Stage {
        RENDER_PASS_STAGE_NONE = 0,
        RENDER_PASS_STAGE_PRESENT = 1, /* for presentation on screen */
        RENDER_PASS_STAGE_SHADER = 2  /* for use as a sampled texture in a shader */
    };

    struct Attachment {
        Texture::TextureInternalFormat format;
    };

    explicit RenderPass(Stage stage);
    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    ~RenderPass();

    inline Stage GetStage() const { return m_stage; }

    inline void AddAttachment(const Attachment &attachment)
    {
        m_attachments.push_back(attachment);
    }

    inline const std::vector<Attachment> &GetAttachments() const { return m_attachments; }

    void Create(Instance *instance);
    void Destroy(Instance *instance);

private:
    void CreateAttachments();
    void CreateDependencies();

    Stage m_stage;
    std::vector<Attachment> m_attachments;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2__RENDER_PASS_H

