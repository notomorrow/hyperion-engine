/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP
#define HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>
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
class Frame;

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <PlatformType PLATFORM>
struct CommandBufferPlatformImpl;

template <PlatformType PLATFORM>
class CommandBuffer
{
public:
    static constexpr PlatformType platform = PLATFORM;

    HYP_API CommandBuffer(CommandBufferType type);
    CommandBuffer(const CommandBuffer &other)               = delete;
    CommandBuffer &operator=(const CommandBuffer &other)    = delete;
    HYP_API ~CommandBuffer();

    HYP_FORCE_INLINE CommandBufferPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const CommandBufferPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE CommandBufferType GetType() const
        { return m_type; }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create(Device<PLATFORM> *device);
    HYP_API RendererResult Destroy(Device<PLATFORM> *device);
    HYP_API RendererResult Begin(Device<PLATFORM> *device, const RenderPass<PLATFORM> *render_pass = nullptr);
    HYP_API RendererResult End(Device<PLATFORM> *device);
    HYP_API RendererResult Reset(Device<PLATFORM> *device);
    HYP_API RendererResult SubmitPrimary(
        DeviceQueue<PLATFORM> *queue,
        Fence<PLATFORM> *fence,
        SemaphoreChain *semaphore_chain
    );

    HYP_API RendererResult SubmitSecondary(CommandBuffer *primary);

    HYP_API void BindVertexBuffer(const GPUBuffer<PLATFORM> *buffer);
    HYP_API void BindIndexBuffer(const GPUBuffer<PLATFORM> *buffer, DatumType datum_type = DatumType::UNSIGNED_INT);

    HYP_API void DrawIndexed(
        uint32 num_indices,
        uint32 num_instances = 1,
        uint32 instance_index = 0
    ) const;

    HYP_API void DrawIndexedIndirect(
        const GPUBuffer<PLATFORM> *buffer,
        uint32 buffer_offset
    ) const;

    HYP_API void DebugMarkerBegin(const char *marker_name) const;
    HYP_API void DebugMarkerEnd() const;

    template <class LambdaFunction>
    RendererResult Record(Device<PLATFORM> *device, const RenderPass<PLATFORM> *render_pass, LambdaFunction &&fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        RendererResult result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    CommandBufferPlatformImpl<PLATFORM> m_platform_impl;
    CommandBufferType                   m_type;
};

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