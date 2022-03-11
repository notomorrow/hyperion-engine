#ifndef HYPERION_RENDERER_FRAME_HANDLER_H
#define HYPERION_RENDERER_FRAME_HANDLER_H

#include "renderer_frame.h"
#include "renderer_swapchain.h"
#include "renderer_command_buffer.h"

#include <vector>
#include <memory>

namespace hyperion {
namespace renderer {

class FrameHandler {
public:
    using NextImageFunction_t = std::add_pointer_t<void(Device *device, Swapchain *swapchain, Frame *frame, uint32_t *image_index)>;

    FrameHandler(NextImageFunction_t next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    inline void AddCommandBuffer(std::unique_ptr<CommandBuffer> &&command_buffer)
        { m_command_buffers.push_back(std::move(command_buffer)); }

    inline Frame *GetCurrentFrame() { return m_allocated_frames[m_image_index].get(); }
    inline const Frame *GetCurrentFrame() const { return m_allocated_frames[m_image_index].get(); }

    Result AllocateFrames(Device *device);
    Result AcquireNextImage(Device *device, Swapchain *swapchain = nullptr);
    Frame *Next();

private:
    std::vector<std::unique_ptr<CommandBuffer>> m_command_buffers;
    std::vector<std::unique_ptr<Frame>> m_allocated_frames;
    NextImageFunction_t m_next_image;
    uint32_t m_image_index;
    size_t m_image_count;
};

} // namespace renderer
} // namespace hyperion

#endif