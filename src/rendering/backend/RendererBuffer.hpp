#ifndef HYPERION_V2_BACKEND_RENDERER_BUFFER_H
#define HYPERION_V2_BACKEND_RENDERER_BUFFER_H

#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

enum class ResourceState : UInt
{
    UNDEFINED,
    PRE_INITIALIZED,
    COMMON,
    VERTEX_BUFFER,
    CONSTANT_BUFFER,
    INDEX_BUFFER,
    RENDER_TARGET,
    UNORDERED_ACCESS,
    DEPTH_STENCIL,
    SHADER_RESOURCE,
    STREAM_OUT,
    INDIRECT_ARG,
    COPY_DST,
    COPY_SRC,
    RESOLVE_DST,
    RESOLVE_SRC,
    PRESENT,
    READ_GENERIC,
    PREDICATION
};


enum class GPUBufferType
{
    NONE = 0,
    MESH_INDEX_BUFFER,
    MESH_VERTEX_BUFFER,
    CONSTANT_BUFFER,
    STORAGE_BUFFER,
    ATOMIC_COUNTER,
    STAGING_BUFFER,
    INDIRECT_ARGS_BUFFER,
    SHADER_BINDING_TABLE,
    ACCELERATION_STRUCTURE_BUFFER,
    ACCELERATION_STRUCTURE_INSTANCE_BUFFER,
    RT_MESH_INDEX_BUFFER,
    RT_MESH_VERTEX_BUFFER,
    SCRATCH_BUFFER
};

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererBuffer.hpp>
#else
#error Unsupported rendering backend
#endif

#endif