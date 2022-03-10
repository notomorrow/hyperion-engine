#ifndef HYPERION_RENDERER_DESCRIPTOR_H
#define HYPERION_RENDERER_DESCRIPTOR_H

#include "renderer_buffer.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_image.h"
#include "../uniform_buffer.h"

#include "../../util/non_owning_ptr.h"

#include <vector>

namespace hyperion {
namespace renderer {
class Device;

class Descriptor {
public:
    enum class Mode {
        UNSET,
        BUFFER,
        IMAGE
    };

    struct BufferInfo {
        Mode mode;

        VkDescriptorBufferInfo buffer_info;
        non_owning_ptr<GPUBuffer> gpu_buffer;

        VkDescriptorImageInfo image_info;
        non_owning_ptr<ImageView> image_view;
        non_owning_ptr<Sampler> sampler;

        BufferInfo()
            : mode(Mode::UNSET),
              buffer_info{},
              gpu_buffer(nullptr),
              image_info{},
              image_view(nullptr),
              sampler(nullptr)
        {}

        explicit BufferInfo(non_owning_ptr<GPUBuffer> gpu_buffer)
            : mode(Mode::BUFFER),
              buffer_info{},
              gpu_buffer(gpu_buffer),
              image_info{},
              image_view(nullptr),
              sampler(nullptr)
        {}

        explicit BufferInfo(non_owning_ptr<ImageView> image_view, non_owning_ptr<Sampler> sampler)
            : mode(Mode::IMAGE),
              buffer_info{},
              gpu_buffer(nullptr),
              image_info{},
              image_view(image_view),
              sampler(sampler)
        {}

        BufferInfo(const BufferInfo &other)
            : mode(other.mode),
              buffer_info(other.buffer_info),
              gpu_buffer(other.gpu_buffer),
              image_info(other.image_info),
              image_view(other.image_view),
              sampler(other.sampler)
        {}

        BufferInfo &operator=(const BufferInfo &other)
        {
            mode = other.mode;
            buffer_info = other.buffer_info;
            gpu_buffer = other.gpu_buffer;
            image_info = other.image_info;
            image_view = other.image_view;
            sampler = other.sampler;

            return *this;
        }

        ~BufferInfo() = default;
    };

    struct Info {
        VkDescriptorSetLayoutBinding binding;
        VkWriteDescriptorSet write;
    };

    Descriptor(uint32_t binding, const BufferInfo &info, VkDescriptorType type, VkShaderStageFlags stage_flags);
    Descriptor(const Descriptor &other) = delete;
    Descriptor &operator=(const Descriptor &other) = delete;
    ~Descriptor();

    inline uint32_t GetBinding() const { return m_binding; }
    inline void SetBinding(uint32_t binding) { m_binding = binding; }

    inline GPUBuffer *GetGPUBuffer() { return m_info.gpu_buffer.get(); }
    inline const GPUBuffer *GetGPUBuffer() const { return m_info.gpu_buffer.get(); }
    inline ImageView *GetImageView() { return m_info.image_view.get(); }
    inline const ImageView *GetImageView() const { return m_info.image_view.get(); }
    inline Sampler *GetSampler() { return m_info.sampler.get(); }
    inline const Sampler *GetSampler() const { return m_info.sampler.get(); }

    void Create(Device *device, Info *out_info);
    void Destroy(Device *device);

protected:
    BufferInfo m_info;
    
    uint32_t m_binding;
    VkDescriptorType m_type;
    VkShaderStageFlags m_stage_flags;
};

class BufferDescriptor : public Descriptor {
public:
    BufferDescriptor(uint32_t binding,
        non_owning_ptr<GPUBuffer> gpu_buffer,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags)
        : Descriptor(
            binding,
            BufferInfo(gpu_buffer),
            type,
            stage_flags
        )
    {}

    BufferDescriptor(const BufferDescriptor &other) = delete;
    BufferDescriptor &operator=(const BufferDescriptor &other) = delete;
    ~BufferDescriptor() = default;
};

class ImageSamplerDescriptor : public Descriptor {
public:
    ImageSamplerDescriptor(uint32_t binding,
        non_owning_ptr<ImageView> image_view,
        non_owning_ptr<Sampler> sampler,
        VkShaderStageFlags stage_flags)
        : Descriptor(
            binding,
            BufferInfo(image_view, sampler),
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            stage_flags) {}
    ImageSamplerDescriptor(const ImageSamplerDescriptor &other) = delete;
    ImageSamplerDescriptor &operator=(const ImageSamplerDescriptor &other) = delete;
    ~ImageSamplerDescriptor() = default;
};

} // namespace renderer
} // namespace hyperion

#endif