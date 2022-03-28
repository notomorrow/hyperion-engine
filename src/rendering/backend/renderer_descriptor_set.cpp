#include "renderer_descriptor_set.h"
#include "renderer_command_buffer.h"
#include "renderer_graphics_pipeline.h"
#include "renderer_compute_pipeline.h"
#include "renderer_buffer.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"

namespace hyperion {
namespace renderer {
DescriptorSet::DescriptorSet()
    : m_state(DescriptorSetState::DESCRIPTOR_DIRTY)
{
}

DescriptorSet::~DescriptorSet()
{
}

Result DescriptorSet::Create(Device *device, DescriptorPool *pool)
{
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_descriptors.size());

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_descriptors.size());

    for (auto &descriptor : m_descriptors) {
        Descriptor::Info info{};

        descriptor->Create(device, &info);

        bindings.push_back(info.binding);
        writes.push_back(info.write);
    }

    //build layout first
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = nullptr;
    layout_info.pBindings = bindings.data();
    layout_info.bindingCount = uint32_t(bindings.size());

    VkDescriptorSetLayout layout;

    {
        auto layout_result = pool->CreateDescriptorSetLayout(device, &layout_info, &layout);

        if (!layout_result) {
            DebugLog(LogType::Error, "Failed to create descriptor set layout! Message was: %s\n", layout_result.message);

            return layout_result;
        }
    }

    {
        auto allocate_result = pool->AllocateDescriptorSet(device, &layout, this);

        if (!allocate_result) {
            DebugLog(LogType::Error, "Failed to allocate descriptor set! Message was: %s\n", allocate_result.message);

            return allocate_result;
        }
    }

    //write descriptor
    for (VkWriteDescriptorSet &w : writes) {
        w.dstSet = m_set;
    }

    vkUpdateDescriptorSets(device->GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    m_state = DescriptorSetState::DESCRIPTOR_CLEAN;

    for (auto &descriptor : m_descriptors) {
        descriptor->m_descriptor_set = non_owning_ptr(this);
        descriptor->SetState(DescriptorSetState::DESCRIPTOR_CLEAN);
    }

    HYPERION_RETURN_OK;
}

Result DescriptorSet::Destroy(Device *device)
{
    for (auto &descriptor : m_descriptors) {
        descriptor->Destroy(device);
    }

    // TODO: clear descriptor set

    HYPERION_RETURN_OK;
}

Result DescriptorSet::Update(Device *device)
{
    DebugLog(LogType::Debug, "Update descriptor set\n");
    // TODO: cache the writes array
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_descriptors.size());

    for (auto &descriptor : m_descriptors) {
        if (descriptor->GetState() != DescriptorSetState::DESCRIPTOR_DIRTY) {
            continue;
        }

        Descriptor::Info info{};

        /* NOTE: This doesn't actually 'create' anything, just sets up the update structs */
        descriptor->Create(device, &info);
        
        writes.push_back(info.write);

        descriptor->SetState(DescriptorSetState::DESCRIPTOR_DIRTY);
    }

    //write descriptor
    for (VkWriteDescriptorSet &w : writes) {
        w.dstSet = m_set;
    }

    vkUpdateDescriptorSets(device->GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    HYPERION_RETURN_OK;
}

const std::unordered_map<VkDescriptorType, size_t> DescriptorPool::items_per_set{
    { VK_DESCRIPTOR_TYPE_SAMPLER, 20 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 40 }, /* sampling imageviews in shader */
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 20 },          /* imageStore, imageLoad etc */
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20 },         /* standard uniform buffer */
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 20 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20 }
};

DescriptorPool::DescriptorPool()
    : m_descriptor_pool(nullptr),
      m_num_descriptor_sets(0),
      m_descriptor_sets_view(new VkDescriptorSet[DescriptorSet::max_descriptor_sets])
{
}

DescriptorPool::~DescriptorPool()
{
    AssertExitMsg(m_descriptor_pool == nullptr, "descriptor pool should have been destroyed!");

    delete[] m_descriptor_sets_view;
}

DescriptorSet &DescriptorPool::AddDescriptorSet()
{
    AssertThrowMsg(m_num_descriptor_sets + 1 <= DescriptorSet::max_descriptor_sets, "Maximum number of descriptor sets added");

    m_descriptor_sets[m_num_descriptor_sets++] = std::make_unique<DescriptorSet>();

    return *m_descriptor_sets[m_num_descriptor_sets - 1];
}

Result DescriptorPool::Create(Device *device)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(items_per_set.size());

    for (auto &it : items_per_set) {
        pool_sizes.push_back({ it.first, uint32_t(it.second * DescriptorSet::max_descriptor_sets) });
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = DescriptorSet::max_descriptor_sets;
    pool_info.poolSizeCount = uint32_t(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    HYPERION_VK_CHECK_MSG(
        vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &m_descriptor_pool),
        "Could not create descriptor pool!"
    );

    size_t index = 0;

    for (uint8_t i = 0; i < m_num_descriptor_sets; i++) {
        AssertThrow(m_descriptor_sets[i] != nullptr);

        auto descriptor_set_result = m_descriptor_sets[i]->Create(device, this);

        if (!descriptor_set_result) return descriptor_set_result;

        m_descriptor_sets_view[index++] = m_descriptor_sets[i]->m_set;
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Destroy(Device *device)
{
    /* Destroy set layouts */
    for (auto &layout : m_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(device->GetDevice(), layout, nullptr);
    }

    m_descriptor_set_layouts.clear();

    /* Destroy sets */

    for (auto &set : m_descriptor_sets) {
        if (set != nullptr) {
            set->Destroy(device);
        }
    }

    vkFreeDescriptorSets(device->GetDevice(), m_descriptor_pool, m_num_descriptor_sets, m_descriptor_sets_view);

    m_descriptor_sets = {};

    // set all to nullptr
    std::memset(m_descriptor_sets_view, 0, sizeof(VkDescriptorSet *) * DescriptorSet::max_descriptor_sets);

    /* Destroy pool */
    vkDestroyDescriptorPool(device->GetDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = nullptr;

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Bind(CommandBuffer *cmd, GraphicsPipeline *pipeline, DescriptorSetBinding &&binding) const
{
    vkCmdBindDescriptorSets(
        cmd->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout,
        binding.locations.binding,
        binding.declaration.count,
        &m_descriptor_sets_view[binding.declaration.set],
        uint32_t(binding.offsets.offsets.size()),
        binding.offsets.offsets.data()
    );

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Bind(CommandBuffer *cmd, ComputePipeline *pipeline, DescriptorSetBinding &&binding) const
{
    vkCmdBindDescriptorSets(
        cmd->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        binding.locations.binding,
        binding.declaration.count,
        &m_descriptor_sets_view[binding.declaration.set],
        uint32_t(binding.offsets.offsets.size()),
        binding.offsets.offsets.data()
    );

    HYPERION_RETURN_OK;
}

Result DescriptorPool::CreateDescriptorSetLayout(Device *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out)
{
    if (vkCreateDescriptorSetLayout(device->GetDevice(), layout_create_info, nullptr, out) != VK_SUCCESS) {
        return Result(Result::RENDERER_ERR, "Could not create descriptor set layout");
    }

    //AssertThrowMsg(m_descriptor_set_layouts.size() < 4, "Maximum number of descriptor sets (to be used) surpassed: " \
    //                                                    "For compatibility, maintain a limit of 4 descriptor sets in use.");

    m_descriptor_set_layouts.push_back(*out);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::DestroyDescriptorSetLayout(Device *device, VkDescriptorSetLayout *layout)
{
    auto it = std::find(m_descriptor_set_layouts.begin(), m_descriptor_set_layouts.end(), *layout);

    if (it == m_descriptor_set_layouts.end()) {
        return Result(Result::RENDERER_ERR, "Could not destroy descriptor set layout; not found in list");
    }

    vkDestroyDescriptorSetLayout(device->GetDevice(), *layout, nullptr);

    m_descriptor_set_layouts.erase(it);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.pSetLayouts = layout;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;

    VkResult alloc_result = vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &out->m_set);

    switch (alloc_result) {
    case VK_SUCCESS: return Result::OK;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        // TODO: re-allocation
        return Result(Result::RENDERER_ERR_NEEDS_REALLOCATION, "Needs reallocation");
    default:
        return Result(Result::RENDERER_ERR, "Unknown error");
    }
}

Descriptor::Descriptor(uint32_t binding, Mode mode, VkShaderStageFlags stage_flags)
    : m_binding(binding),
      m_mode(mode),
      m_stage_flags(stage_flags),
      m_state(DescriptorSetState::DESCRIPTOR_DIRTY)
{
}

Descriptor::~Descriptor()
{
}

void Descriptor::Create(Device *device, Descriptor::Info *out_info)
{
    uint32_t num_descriptors = 0;

    for (auto &sub : m_sub_descriptors) {
        switch (m_mode) {
        case Mode::UNIFORM_BUFFER: /* fallthrough */
        case Mode::UNIFORM_BUFFER_DYNAMIC:
        case Mode::STORAGE_BUFFER:
        case Mode::STORAGE_BUFFER_DYNAMIC:
            AssertThrow(sub.gpu_buffer != nullptr);
            AssertThrow(sub.gpu_buffer->buffer != nullptr);

            m_sub_descriptor_buffer.buffers.push_back(VkDescriptorBufferInfo{
                .buffer = sub.gpu_buffer->buffer,
                .offset = 0,
                .range = sub.range ? sub.range : sub.gpu_buffer->size
            });

            ++num_descriptors;

            break;
        case Mode::IMAGE_SAMPLER:
            AssertThrow(sub.image_view != nullptr);
            AssertThrow(sub.image_view->GetImageView() != nullptr);

            AssertThrow(sub.sampler != nullptr);
            AssertThrow(sub.sampler->GetSampler() != nullptr);

            m_sub_descriptor_buffer.images.push_back(VkDescriptorImageInfo{
                .sampler = sub.sampler->GetSampler(),
                .imageView = sub.image_view->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });

            ++num_descriptors;

            break;
        case Mode::IMAGE_STORAGE:
            AssertThrow(sub.image_view != nullptr);
            AssertThrow(sub.image_view->GetImageView() != nullptr);

            m_sub_descriptor_buffer.images.push_back(VkDescriptorImageInfo{
                .sampler = nullptr,
                .imageView = sub.image_view->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL
            });

            ++num_descriptors;


            break;
        default:
            AssertThrowMsg(false, "unhandled descriptor type");
        }
    }

    const auto descriptor_type = GetDescriptorType(m_mode);

    out_info->binding = VkDescriptorSetLayoutBinding{};
    out_info->binding.descriptorCount = num_descriptors;
    out_info->binding.descriptorType = descriptor_type;
    out_info->binding.pImmutableSamplers = nullptr;
    out_info->binding.stageFlags = m_stage_flags;
    out_info->binding.binding = m_binding;

    out_info->write = VkWriteDescriptorSet{};
    out_info->write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    out_info->write.pNext = nullptr;
    out_info->write.descriptorCount = num_descriptors;
    out_info->write.descriptorType = descriptor_type;
    out_info->write.pBufferInfo = m_sub_descriptor_buffer.buffers.data();
    out_info->write.pImageInfo = m_sub_descriptor_buffer.images.data();
    out_info->write.dstBinding = m_binding;
}

void Descriptor::Destroy(Device *device)
{
}

VkDescriptorType Descriptor::GetDescriptorType(Mode mode)
{
    switch (mode) {
    case Mode::UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case Mode::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case Mode::STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case Mode::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case Mode::IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case Mode::IMAGE_STORAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type");
    }
}


} // namespace renderer
} // namespace hyperion