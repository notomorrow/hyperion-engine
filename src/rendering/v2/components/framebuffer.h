#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "base.h"
#include "texture.h"

#include <rendering/backend/renderer_fbo.h>

namespace hyperion::v2 {

using Framebuffer = EngineComponent<renderer::FramebufferObject>;

/*class Framebuffer : public EngineComponent<renderer::FramebufferObject> {
public:
    Framebuffer();
    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
};*/

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
