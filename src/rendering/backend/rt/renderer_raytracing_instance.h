#ifndef HYPERION_V2_BACKEND_RENDERER_RAYTRACING_INSTANCE_H
#define HYPERION_V2_BACKEND_RENDERER_RAYTRACING_INSTANCE_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/renderer_raytracing_instance.h>
#else
#error Unsupported rendering backend
#endif

#endif