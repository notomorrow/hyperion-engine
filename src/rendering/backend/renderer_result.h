#ifndef HYPERION_V2_BACKEND_RENDERER_RESULT_H
#define HYPERION_V2_BACKEND_RENDERER_RESULT_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_result.h>
#else
#error Unsupported rendering backend
#endif

#endif