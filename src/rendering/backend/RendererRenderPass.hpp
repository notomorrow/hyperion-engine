/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_RENDER_PASS_HPP
#define HYPERION_BACKEND_RENDERER_RENDER_PASS_HPP

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

enum RenderPassMode
{
    RENDER_PASS_INLINE = 0,
    RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
};

namespace platform {

template <PlatformType PLATFORM>
class RenderPass
{
public:
    static constexpr PlatformType platform = PLATFORM;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using RenderPass = platform::RenderPass<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif