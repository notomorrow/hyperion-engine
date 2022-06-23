#ifndef HYPERION_V2_RENDER_PASS_H
#define HYPERION_V2_RENDER_PASS_H

#include "base.h"
#include <types.h>

#include <rendering/backend/renderer_render_pass.h>

namespace hyperion::v2 {

using renderer::RenderPassStage;

class RenderPass : public EngineComponentBase<STUB_CLASS(RenderPass)> {
public:
    RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode);
    RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode, UInt num_multiview_layers);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    renderer::RenderPass &GetRenderPass()             { return m_render_pass; }
    const renderer::RenderPass &GetRenderPass() const { return m_render_pass; }

    void Init(Engine *engine);

private:
    renderer::RenderPass m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2__RENDER_PASS_H

