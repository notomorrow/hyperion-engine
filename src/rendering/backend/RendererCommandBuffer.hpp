/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP
#define HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

enum CommandBufferType
{
    COMMAND_BUFFER_PRIMARY,
    COMMAND_BUFFER_SECONDARY
};

namespace platform {

template <PlatformType PLATFORM>
class CommandBuffer { };

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using CommandBuffer = platform::CommandBuffer<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif