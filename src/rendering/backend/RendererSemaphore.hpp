/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SEMAPHORE_HPP
#define HYPERION_BACKEND_RENDERER_SEMAPHORE_HPP

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

enum class SemaphoreType
{
    WAIT,
    SIGNAL
};

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#else
#error Unsupported rendering backend
#endif

#endif