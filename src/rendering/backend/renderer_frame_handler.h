#ifndef HYPERION_RENDERER_FRAME_HANDLER_H
#define HYPERION_RENDERER_FRAME_HANDLER_H

#include "renderer_frame.h"
#include "renderer_swapchain.h"
#include "renderer_command_buffer.h"

#include <vector>
#include <memory>
#include <utility>
#include <tuple>

namespace hyperion {
namespace renderer {

template<class T>
class FrameDataSingle {};

template<class T, class ...Args>
class FrameData : public FrameDataSingle<Args>... {
public:
    FrameData() = delete;
};

template<class ...Args>
class FrameData<CommandBuffer, Args...> : public FrameData<Args>... {
public:
    CommandBuffer *GetCommandBuffer()
        { return m_command_buffer.get(); }
    const CommandBuffer *GetCommandBuffer() const
        { return m_command_buffer.get(); }
    void SetCommandBuffer(std::unique_ptr<CommandBuffer> &&command_buffer)
        { m_command_buffer = std::move(command_buffer); }

private:
    std::unique_ptr<CommandBuffer> m_command_buffer;
};

template<class ...Args>
class FrameData<Frame, Args...> : public FrameData<Args>... {
public:
    Frame *GetFrame()
        { return m_frame.get(); }
    const Frame *GetFrame() const
        { return m_frame.get(); }
    void SetFrame(std::unique_ptr<Frame> &&frame)
        { m_frame = std::move(frame); }

private:
    std::unique_ptr<Frame> m_frame;
};

template<class ...Args>
class FrameData<SemaphoreChain, Args...> : public FrameData<Args>... {
public:
    SemaphoreChain *GetSemaphoreChain()
        { return m_semaphore_chain.get(); }
    const SemaphoreChain *GetSemaphoreChain() const
        { return m_semaphore_chain.get(); }
    void SetSemaphoreChain(std::unique_ptr<SemaphoreChain> &&semaphore_chain)
        { m_semaphore_chain = std::move(semaphore_chain); }

private:
    std::unique_ptr<SemaphoreChain> m_semaphore_chain;
};

template<class ...Args>
class FrameData<Semaphore, Args...> : public FrameData<Args>... {
public:
    Semaphore *GetSemaphore()
        { return m_semaphore.get(); }
    const Semaphore *GetSemaphore() const
        { return m_semaphore.get(); }
    void SetSemaphore(std::unique_ptr<Semaphore> &&semaphore)
        { m_semaphore = std::move(semaphore); }

private:
    std::unique_ptr<Semaphore> m_semaphore;
};

template<class ...Args>
class PerFrameData {
    using FrameData_t = FrameData<Args...>;
public:
    PerFrameData(size_t num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(size_t num_frames, std::function<Args&&()> &&... fn) : m_num_frames(num_frames)
    {
        m_data(num_frames, std::move(std::apply(std::move(fn())))...);
    }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    FrameData_t &operator[](size_t index)
        { return m_data[index]; }
    const FrameData_t &operator[](size_t index) const
        { return m_data[index]; }

    FrameData_t &GetFrame(size_t index)
        { return operator[](index); }

    const FrameData_t &GetFrame(size_t index) const
        { return operator[](index); }

    inline size_t GetNumFrames() const
        { return m_num_frames; }

    inline void Reset()
        { m_data = std::vector<FrameData_t>(m_num_frames); }

protected:
    size_t m_num_frames;
    std::vector<FrameData_t> m_data;
};

class FrameHandler {
public:
    using NextImageFunction = std::add_pointer_t<VkResult(Device *device, Swapchain *swapchain, Frame *frame, uint32_t *image_index)>;

    FrameHandler(size_t num_frames, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    inline auto &GetPerFrameData() { return m_per_frame_data; }
    inline const auto &GetPerFrameData() const { return m_per_frame_data; }

    inline auto &GetCurrentFrameData() { return m_per_frame_data[m_frame_index]; }
    inline const auto &GetCurrentFrameData() const { return m_per_frame_data[m_frame_index]; }

    inline size_t GetNumFrames() const { return m_num_frames; }
    inline uint32_t GetFrameIndex() const { return m_frame_index; }

    /* Optionally used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    Result AcquireNextImage(Device *device, Swapchain *swapchain, VkResult *out_result);
    /* Create our Frame objects (count is same as num_frames) */
    Result CreateFrames(Device *device);
    /* Create our CommandBuffer objects (count is same as num_frames) */
    Result CreateCommandBuffers(Device *device, VkCommandPool pool);
    
    Result Destroy(Device *device, VkCommandPool pool);

private:
    PerFrameData<CommandBuffer, Frame> m_per_frame_data;

    NextImageFunction m_next_image;
    uint32_t m_frame_index;
    size_t m_num_frames;
};

} // namespace renderer
} // namespace hyperion

#endif