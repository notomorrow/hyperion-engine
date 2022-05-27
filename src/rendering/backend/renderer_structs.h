#ifndef HYPERION_V2_BACKEND_RENDERER_STRUCTS_H
#define HYPERION_V2_BACKEND_RENDERER_STRUCTS_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_structs.h>
#else
#error Unsupported rendering backend
#endif

#endif