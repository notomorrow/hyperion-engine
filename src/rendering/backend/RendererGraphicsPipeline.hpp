/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>

#include <math/Vertex.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class GraphicsPipeline : public Pipeline<PLATFORM>
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using GraphicsPipeline = platform::GraphicsPipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
