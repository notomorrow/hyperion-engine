/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FRAME_HPP
#define HYPERION_BACKEND_RENDERER_FRAME_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Fence;

template <PlatformType PLATFORM>
class Swapchain;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct DeviceQueue;

template <PlatformType PLATFORM>
class Frame
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    static Frame TemporaryFrame(CommandBufferRef<PLATFORM> command_buffer, uint32 frame_index = 0)
    {
        Frame frame;
        frame.m_command_buffer = std::move(command_buffer);
        frame.m_frame_index = frame_index;
        return frame;
    }

    explicit Frame();
    Frame(uint32 frame_index);
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    Frame(Frame &&other) noexcept;
    ~Frame();

    RendererResult Create(Device<PLATFORM> *device, CommandBufferRef<PLATFORM> cmd);
    RendererResult Destroy(Device<PLATFORM> *device);

    HYP_FORCE_INLINE const FenceRef<PLATFORM> &GetFence() const
        { return m_queue_submit_fence; }

    HYP_FORCE_INLINE uint32 GetFrameIndex() const
        { return m_frame_index; }
    
    HYP_FORCE_INLINE const CommandBufferRef<PLATFORM> &GetCommandBuffer() const
        { return m_command_buffer; }

    HYP_FORCE_INLINE SemaphoreChain &GetPresentSemaphores()
        { return m_present_semaphores; }

    HYP_FORCE_INLINE const SemaphoreChain &GetPresentSemaphores() const
        { return m_present_semaphores; }

    /* Start recording into the command buffer */
    RendererResult BeginCapture(Device<PLATFORM> *device);
    /* Stop recording into the command buffer */
    RendererResult EndCapture(Device<PLATFORM> *device);
    /* Submit command buffer to the given queue */
    RendererResult Submit(DeviceQueue<PLATFORM> *queue);

    RendererResult RecreateFence(Device<PLATFORM> *device);
    
    CommandBufferRef<PLATFORM>  m_command_buffer;

private:
    uint32              m_frame_index;
    SemaphoreChain      m_present_semaphores;
    FenceRef<PLATFORM>  m_queue_submit_fence;
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrame.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Frame = platform::Frame<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif