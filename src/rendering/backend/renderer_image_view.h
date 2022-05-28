#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_image_view.h>
#else
#error Unsupported rendering backend
#endif

#endif