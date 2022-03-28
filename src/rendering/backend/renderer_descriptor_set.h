#ifndef HYPERION_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_RENDERER_DESCRIPTOR_SET_H

#include "renderer_result.h"

#include <util/non_owning_ptr.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;
class GraphicsPipeline;
class ComputePipeline;
class ImageView;
class Sampler;
class GPUBuffer;
class Descriptor;
class DescriptorPool;

enum class DescriptorSetState {
    DESCRIPTOR_CLEAN = 0,
    DESCRIPTOR_DIRTY = 1
};

class DescriptorSet {
    friend class Descriptor;
public:
    enum Index {
        DESCRIPTOR_SET_INDEX_GLOBALS  = 0, /* per frame */
        DESCRIPTOR_SET_INDEX_PASS     = 1, /* per render pass */
        DESCRIPTOR_SET_INDEX_SCENE    = 2, /* per scene */
        DESCRIPTOR_SET_INDEX_OBJECT   = 3, /* per object */
        DESCRIPTOR_SET_INDEX_SCENE_FRAME_1  = 4,
        DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1 = 5
    };

    static constexpr uint8_t max_descriptor_sets = 6;

    DescriptorSet();
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    ~DescriptorSet();

    inline DescriptorSetState GetState() const { return m_state; }

    template <class DescriptorType, class ...Args>
    Descriptor *AddDescriptor(Args &&... args)
    {
        m_descriptors.push_back(std::make_unique<DescriptorType>(std::move(args)...));

        return m_descriptors.back().get();
    }

    inline Descriptor *GetDescriptor(size_t index) { return m_descriptors[index].get(); }
    inline const Descriptor *GetDescriptor(size_t index) const { return m_descriptors[index].get(); }
    inline std::vector<std::unique_ptr<Descriptor>> &GetDescriptors() { return m_descriptors; }
    inline const std::vector<std::unique_ptr<Descriptor>> &GetDescriptors() const { return m_descriptors; }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);
    Result Update(Device *device);

    VkDescriptorSet m_set;

private:
    std::vector<std::unique_ptr<Descriptor>> m_descriptors;
    DescriptorSetState m_state;
};

struct DescriptorSetBinding {
    struct Declaration {
        uint32_t set;
        uint32_t count; /* default to num total sets - set */
    } declaration;

    /* where we bind to in the shader program */
    struct Locations {
        uint32_t binding; /* defaults to first_set_index */
    } locations;

    struct DynamicOffsets {
        std::vector<uint32_t> offsets;
    } offsets;

    DescriptorSetBinding()
        : declaration({ .set = 0, .count = DescriptorSet::max_descriptor_sets }),
          locations({ .binding = 0 })
    {
    }

    DescriptorSetBinding(Declaration &&dec)
        : declaration(std::move(dec)),
          locations({ .binding = dec.set })
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::max_descriptor_sets - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc)
        : declaration(std::move(dec)),
          locations(std::move(loc))
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::max_descriptor_sets - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc, DynamicOffsets &&offsets)
        : DescriptorSetBinding(std::move(dec), std::move(loc))
    {
        this->offsets = std::move(offsets);  // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
};

class DescriptorPool {
    friend class DescriptorSet;

    Result AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out);
    Result CreateDescriptorSetLayout(Device *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    Result DestroyDescriptorSetLayout(Device *device, VkDescriptorSetLayout *layout);

public:
    static const std::unordered_map<VkDescriptorType, size_t> items_per_set;

    struct BufferInfo {
        VkDescriptorImageInfo *image_info;
        VkDescriptorBufferInfo *buffer_info;

        BufferInfo()
            : image_info(nullptr), buffer_info(nullptr) {}
        BufferInfo(VkDescriptorImageInfo *image_info, VkDescriptorBufferInfo *buffer_info)
            : image_info(image_info), buffer_info(buffer_info) {}
        BufferInfo(const BufferInfo &other)
            : image_info(other.image_info), buffer_info(other.buffer_info) {}
        inline BufferInfo &operator=(const BufferInfo &other)
        {
            image_info = other.image_info;
            buffer_info = other.buffer_info;

            return *this;
        }
    };

    DescriptorPool();
    DescriptorPool(const DescriptorPool &other) = delete;
    DescriptorPool &operator=(const DescriptorPool &other) = delete;
    ~DescriptorPool();

    inline uint8_t NumDescriptorSets() const { return m_num_descriptor_sets; }

    inline const std::vector<VkDescriptorSetLayout> &GetDescriptorSetLayouts() const
        { return m_descriptor_set_layouts; }

    // return new descriptor set
    DescriptorSet &AddDescriptorSet();

    inline DescriptorSet *GetDescriptorSet(DescriptorSet::Index index)
        { return m_descriptor_sets[index].get(); }

    inline const DescriptorSet *GetDescriptorSet(DescriptorSet::Index index) const
        { return m_descriptor_sets[index].get(); }

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result Bind(CommandBuffer *cmd, GraphicsPipeline *pipeline, DescriptorSetBinding &&) const;
    Result Bind(CommandBuffer *cmd, ComputePipeline *pipeline, DescriptorSetBinding &&) const;

private:
    std::array<std::unique_ptr<DescriptorSet>, DescriptorSet::max_descriptor_sets> m_descriptor_sets;
    uint8_t m_num_descriptor_sets;
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    VkDescriptorPool m_descriptor_pool;
    VkDescriptorSet *m_descriptor_sets_view;
};

class Descriptor {
    friend class DescriptorSet;
public:
    enum class Mode {
        UNSET,
        UNIFORM_BUFFER,
        UNIFORM_BUFFER_DYNAMIC,
        STORAGE_BUFFER,
        STORAGE_BUFFER_DYNAMIC,
        IMAGE_SAMPLER,
        IMAGE_STORAGE
    };

    struct SubDescriptor {
        GPUBuffer *gpu_buffer = nullptr;
        uint32_t range = 0; /* if 0 then gpu_buffer->size */

        ImageView *image_view = nullptr;
        Sampler *sampler = nullptr;
    };

    struct Info {
        VkDescriptorSetLayoutBinding binding;
        VkWriteDescriptorSet write;
    };

    Descriptor(uint32_t binding, Mode mode, VkShaderStageFlags stage_flags);
    Descriptor(const Descriptor &other) = delete;
    Descriptor &operator=(const Descriptor &other) = delete;
    ~Descriptor();

    inline uint32_t GetBinding() const { return m_binding; }
    inline void SetBinding(uint32_t binding) { m_binding = binding; }

    inline DescriptorSetState GetState() const { return m_state; }
    inline void SetState(DescriptorSetState state) { m_state = state; }

    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    inline const std::vector<SubDescriptor> &GetSubDescriptors() const
        { return m_sub_descriptors; }

    inline Descriptor *AddSubDescriptor(SubDescriptor &&sub_descriptor)
        {  m_sub_descriptors.push_back(sub_descriptor); return this; }

    void Create(Device *device, Info *out_info);
    void Destroy(Device *device);

protected:
    struct BufferInfo {
        std::vector<VkDescriptorBufferInfo> buffers;
        std::vector<VkDescriptorImageInfo>  images;
    };

    static VkDescriptorType GetDescriptorType(Mode mode);

    std::vector<SubDescriptor> m_sub_descriptors;
    BufferInfo m_sub_descriptor_buffer;
    DescriptorSetState m_state;

    uint32_t m_binding;
    Mode m_mode;
    VkShaderStageFlags m_stage_flags;

private:
    non_owning_ptr<DescriptorSet> m_descriptor_set;
};

/* Convenience descriptor classes */

class UniformBufferDescriptor : public Descriptor {
public:
    UniformBufferDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::UNIFORM_BUFFER,
        stage_flags)
    {}
};

class DynamicUniformBufferDescriptor : public Descriptor {
public:
    DynamicUniformBufferDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::UNIFORM_BUFFER_DYNAMIC,
        stage_flags)
    {}
};

class StorageBufferDescriptor : public Descriptor {
public:
    StorageBufferDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::STORAGE_BUFFER,
        stage_flags)
    {}
};

class DynamicStorageBufferDescriptor : public Descriptor {
public:
    DynamicStorageBufferDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::STORAGE_BUFFER_DYNAMIC,
        stage_flags)
    {}
};

class ImageSamplerDescriptor : public Descriptor {
public:
    ImageSamplerDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::IMAGE_SAMPLER,
        stage_flags)
    {}
};

class ImageStorageDescriptor : public Descriptor {
public:
    ImageStorageDescriptor(
        uint32_t binding,
        VkShaderStageFlags stage_flags
    ) : Descriptor(
        binding,
        Mode::IMAGE_STORAGE,
        stage_flags)
    {}
};

} // namespace renderer
} // namespace hyperion

#endif