#ifndef HYPERION_V2_BACKEND_RENDERER_SEMAPHORE_H
#define HYPERION_V2_BACKEND_RENDERER_SEMAPHORE_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_semaphore.h>
#else
#error Unsupported rendering backend
#endif

#endif