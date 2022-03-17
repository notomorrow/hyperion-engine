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

template<class ...Args>
class PerFrameData {
    struct FrameDataWrapper {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(uint32_t num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    inline uint32_t GetNumFrames() const
        { return m_num_frames; }

    FrameDataWrapper &operator[](uint32_t index)
        { return m_data[index]; }
    const FrameDataWrapper &operator[](uint32_t index) const
        { return m_data[index]; }

    inline void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    uint32_t m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

class FrameHandler {
public:
    using NextImageFunction = std::add_pointer_t<VkResult(Device *device, Swapchain *swapchain, Frame *frame, uint32_t *image_index)>;

    FrameHandler(uint8_t num_images, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    inline auto &GetPerFrameData() { return m_per_frame_data; }
    inline const auto &GetPerFrameData() const { return m_per_frame_data; }

    inline auto &GetCurrentFrameData() { return m_per_frame_data[m_current_frame_index]; }
    inline const auto &GetCurrentFrameData() const { return m_per_frame_data[m_current_frame_index]; }

    inline uint32_t GetNumFrames() const { return m_per_frame_data.GetNumFrames(); }
    inline uint32_t GetAcquiredImageIndex() const { return m_acquired_image_index; }
    inline uint32_t GetCurrentFrameIndex() const { return m_current_frame_index; }

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
    uint32_t m_acquired_image_index;
    uint32_t m_current_frame_index;
};

} // namespace renderer
} // namespace hyperion

#endif