#ifndef HYPERION_V2_BACKEND_RENDERER_RENDER_PASS_H
#define HYPERION_V2_BACKEND_RENDERER_RENDER_PASS_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_render_pass.h>
#else
#error Unsupported rendering backend
#endif

#endif