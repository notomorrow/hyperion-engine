#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "post_fx.h"

namespace hyperion::v2 {

class DeferredRendering : public PostEffect {
public:
    DeferredRendering();
    DeferredRendering(const DeferredRendering &other) = delete;
    DeferredRendering &operator=(const DeferredRendering &other) = delete;
    ~DeferredRendering();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFrameData(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index);
};

} // namespace hyperion::v2

#endif