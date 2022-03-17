#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "base.h"

#include <rendering/backend/renderer_fbo.h>

namespace hyperion::v2 {

using Framebuffer = EngineComponent<renderer::FramebufferObject>;

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
