#ifndef HYPERION_V2_RENDER_PASS_H
#define HYPERION_V2_RENDER_PASS_H

#include "base.h"

#include <rendering/backend/renderer_render_pass.h>

#include <memory>

namespace hyperion::v2 {

class RenderPass : public BaseComponent<renderer::RenderPass> {
public:
    explicit RenderPass();
    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    ~RenderPass();

    void Create(Instance *instance);
    void Destroy(Instance *instance);
};

} // namespace hyperion::v2

#endif // !HYPERION_V2__RENDER_PASS_H

