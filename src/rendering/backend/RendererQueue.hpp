/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_QUEUE_HPP
#define HYPERION_BACKEND_RENDERER_QUEUE_HPP

#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

enum class DeviceQueueType : uint32
{
    GRAPHICS,
    COMPUTE,
    TRANSFER,
    PRESENT,
    MAX
};

namespace platform {

template <PlatformType PLATFORM>
struct DeviceQueue
{
    static constexpr PlatformType platform = PLATFORM;
};

} // namespace platform
} // namespace hyperion::renderer
#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererQueue.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion::renderer {

using DeviceQueue = platform::DeviceQueue<Platform::CURRENT>;

} // namespace hyperion::renderer

#endif