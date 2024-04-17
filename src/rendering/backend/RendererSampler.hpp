/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SAMPLER_HPP
#define HYPERION_BACKEND_RENDERER_SAMPLER_HPP

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Sampler { };

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSampler.hpp>
#else
#error Unsupported rendering backend
#endif


namespace hyperion {
namespace renderer {

using Sampler = platform::Sampler<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif