#ifndef HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H
#define HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/renderer_acceleration_structure.h>
#else
#error Unsupported rendering backend
#endif

#endif