#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_descriptor_set.h>
#else
#error Unsupported rendering backend
#endif

#endif