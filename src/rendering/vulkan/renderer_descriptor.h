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

class RendererDevice;

class RendererDescriptor {
public:
    enum class Mode {
        UNSET,
        BUFFER,
        IMAGE
    };

    struct BufferInfo {
        Mode mode;

        VkDescriptorBufferInfo buffer_info;
        non_owning_ptr<RendererGPUBuffer> gpu_buffer;

        VkDescriptorImageInfo image_info;
        non_owning_ptr<RendererGPUImage> gpu_image;
        non_owning_ptr<RendererImageView> image_view;
        non_owning_ptr<RendererSampler> sampler;

        BufferInfo()
            : mode(Mode::UNSET),
              buffer_info{},
              gpu_buffer(nullptr),
              image_info{},
              gpu_image(nullptr),
              image_view(nullptr),
              sampler(nullptr)
        {}

        explicit BufferInfo(non_owning_ptr<RendererGPUBuffer> gpu_buffer)
            : mode(Mode::BUFFER),
              buffer_info{},
              gpu_buffer(gpu_buffer),
              image_info{},
              gpu_image(nullptr),
              image_view(nullptr),
              sampler(nullptr)
        {}

        explicit BufferInfo(non_owning_ptr<RendererImageView> image_view, non_owning_ptr<RendererSampler> sampler)
            : mode(Mode::IMAGE),
              buffer_info{},
              gpu_buffer(nullptr),
              image_info{},
              gpu_image(nullptr),
              image_view(image_view),
              sampler(sampler)
        {}

        BufferInfo(const BufferInfo &other)
            : mode(other.mode),
              buffer_info(other.buffer_info),
              gpu_buffer(other.gpu_buffer),
              image_info(other.image_info),
              gpu_image(other.gpu_image),
              image_view(other.image_view),
              sampler(other.sampler)
        {}
        BufferInfo &operator=(const BufferInfo &other)
        {
            mode = other.mode;
            buffer_info = other.buffer_info;
            gpu_buffer = other.gpu_buffer;
            image_info = other.image_info;
            gpu_image = other.gpu_image;
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

    RendererDescriptor(uint32_t binding, const BufferInfo &info, VkDescriptorType type, VkShaderStageFlags stage_flags);
    RendererDescriptor(const RendererDescriptor &other) = delete;
    RendererDescriptor &operator=(const RendererDescriptor &other) = delete;
    ~RendererDescriptor();

    inline RendererGPUBuffer *GetGPUBuffer() { return m_info.gpu_buffer.get(); }
    inline const RendererGPUBuffer *GetGPUBuffer() const { return m_info.gpu_buffer.get(); }
    inline RendererImageView *GetImageView() { return m_info.image_view.get(); }
    inline const RendererImageView *GetImageView() const { return m_info.image_view.get(); }
    inline RendererSampler *GetSampler() { return m_info.sampler.get(); }
    inline const RendererSampler *GetSampler() const { return m_info.sampler.get(); }

    void Create(RendererDevice *device, Info *out_info);
    void Destroy(RendererDevice *device);

protected:
    BufferInfo m_info;
    
    uint32_t m_binding;
    VkDescriptorType m_type;
    VkShaderStageFlags m_stage_flags;
};

class RendererBufferDescriptor : public RendererDescriptor {
public:
    RendererBufferDescriptor(uint32_t binding,
        non_owning_ptr<RendererGPUBuffer> gpu_buffer,
        VkDescriptorType type,
        VkShaderStageFlags stage_flags)
        : RendererDescriptor(
            binding,
            BufferInfo(gpu_buffer),
            type,
            stage_flags
        )
    {}

    RendererBufferDescriptor(const RendererBufferDescriptor &other) = delete;
    RendererBufferDescriptor &operator=(const RendererBufferDescriptor &other) = delete;
    ~RendererBufferDescriptor() = default;
};

class RendererImageSamplerDescriptor : public RendererDescriptor {
public:
    RendererImageSamplerDescriptor(uint32_t binding,
        non_owning_ptr<RendererImageView> image_view,
        non_owning_ptr<RendererSampler> sampler,
        VkShaderStageFlags stage_flags)
        : RendererDescriptor(
            binding,
            BufferInfo(image_view, sampler),
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            stage_flags) {}
    RendererImageSamplerDescriptor(const RendererImageSamplerDescriptor &other) = delete;
    RendererImageSamplerDescriptor &operator=(const RendererImageSamplerDescriptor &other) = delete;
    ~RendererImageSamplerDescriptor() = default;
};

} // namespace hyperion

#endif