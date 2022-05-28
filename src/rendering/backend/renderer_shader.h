#ifndef HYPERION_V2_BACKEND_RENDERER_SHADER_H
#define HYPERION_V2_BACKEND_RENDERER_SHADER_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_shader.h>
#else
#error Unsupported rendering backend
#endif

#endif