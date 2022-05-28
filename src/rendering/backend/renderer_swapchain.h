#ifndef HYPERION_V2_BACKEND_RENDERER_SWAPCHAIN_H
#define HYPERION_V2_BACKEND_RENDERER_SWAPCHAIN_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_swapchain.h>
#else
#error Unsupported rendering backend
#endif

#endif