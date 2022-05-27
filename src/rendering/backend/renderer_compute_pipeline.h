#ifndef HYPERION_V2_BACKEND_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_COMPUTE_PIPELINE_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_compute_pipeline.h>
#else
#error Unsupported rendering backend
#endif

#endif