/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SHADER_HPP
#define HYPERION_BACKEND_RENDERER_SHADER_HPP

#include <rendering/backend/Platform.hpp>

#include <core/lib/ByteBuffer.hpp>
#include <HashCode.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

struct ShaderObject
{
    ByteBuffer bytes;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes);

        return hc;
    }
};

enum ShaderModuleType : uint
{
    UNSET = 0,

    /* Graphics and general purpose shaders */
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    COMPUTE,

    /* Mesh shaders */
    TASK,
    MESH,

    /* Tesselation */
    TESS_CONTROL,
    TESS_EVAL,

    /* Raytracing hardware specific */
    RAY_GEN,
    RAY_INTERSECT,
    RAY_ANY_HIT,
    RAY_CLOSEST_HIT,
    RAY_MISS,

    MAX
};

namespace platform {

template <PlatformType PLATFORM>
struct ShaderModule;

template <PlatformType PLATFORM>
struct ShaderGroup;

template <PlatformType PLATFORM>
class ShaderProgram
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererShader.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ShaderProgram = platform::ShaderProgram<Platform::CURRENT>;
using ShaderModule = platform::ShaderModule<Platform::CURRENT>;
using ShaderGroup = platform::ShaderGroup<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif