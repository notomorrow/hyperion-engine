#ifndef HYPERION_V2_RENDER_PASS_H
#define HYPERION_V2_RENDER_PASS_H

#include "base.h"

#include <rendering/backend/renderer_render_pass.h>

namespace hyperion::v2 {

using RenderPass = EngineComponent<renderer::RenderPass>;

/*class RenderPass : public EngineComponent<renderer::RenderPass> {
public:
    RenderPass();
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
};*/

} // namespace hyperion::v2

#endif // !HYPERION_V2__RENDER_PASS_H

