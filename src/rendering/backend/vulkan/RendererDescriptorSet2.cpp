#include <rendering/backend/RendererDescriptorSet2.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp> // For DescriptorSetDeclaration

#include <math/MathUtil.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

static VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type)
{
    switch (type) {
    case DescriptorSetElementType::UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorSetElementType::STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorSetElementType::IMAGE:                  return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorSetElementType::SAMPLER:                return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorSetElementType::IMAGE_STORAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorSetElementType::TLAS:                   return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type for Vulkan");
    }
}

struct VulkanDescriptorElementInfo
{
    uint                binding;
    uint                index;
    VkDescriptorType    descriptor_type;

    union {
        VkDescriptorBufferInfo                          buffer_info;
        VkDescriptorImageInfo                           image_info;
        VkWriteDescriptorSetAccelerationStructureKHR    acceleration_structure_info;
    };
};
struct VulkanDescriptorSetLayoutWrapper
{
    VkDescriptorSetLayout   vk_layout = VK_NULL_HANDLE;

    Result Create(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
    {
        AssertThrow(vk_layout == VK_NULL_HANDLE);

        static constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        Array<VkDescriptorSetLayoutBinding> bindings;
        bindings.Reserve(layout.GetElements().Size());

        Array<VkDescriptorBindingFlags> binding_flags;
        binding_flags.Reserve(layout.GetElements().Size());

        for (const auto &it : layout.GetElements()) {
            const String &name = it.first;
            const DescriptorSetLayoutElement &element = it.second;

            uint32 descriptor_count = element.count;

            if (element.IsBindless()) {
                descriptor_count = max_bindless_resources;
            }

            VkDescriptorSetLayoutBinding binding { };
            binding.descriptorCount = descriptor_count;
            binding.descriptorType = ToVkDescriptorType(element.type);
            binding.pImmutableSamplers = nullptr;
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.binding = element.binding;

            bindings.PushBack(binding);

            VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

            if (element.IsBindless()) {
                flags |= bindless_flags;
            }

            binding_flags.PushBack(flags);
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        extended_info.bindingCount = uint32(binding_flags.Size());
        extended_info.pBindingFlags = binding_flags.Data();

        VkDescriptorSetLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layout_info.pBindings = bindings.Data();
        layout_info.bindingCount = uint32(bindings.Size());
        layout_info.flags = 0;
        layout_info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_info.pNext = &extended_info;

        HYPERION_VK_CHECK(vkCreateDescriptorSetLayout(
            device->GetDevice(),
            &layout_info,
            nullptr,
            &vk_layout
        ));

        return Result::OK;
    }

    Result Destroy(Device<Platform::VULKAN> *device)
    {
        AssertThrow(vk_layout != VK_NULL_HANDLE);

        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            vk_layout,
            nullptr
        );

        vk_layout = VK_NULL_HANDLE;

        return Result::OK;
    }
};

template <>
DescriptorSetLayout<Platform::VULKAN>::DescriptorSetLayout(const DescriptorSetDeclaration &decl)
{
    const DescriptorSetDeclaration *decl_ptr = &decl;

    if (decl.is_reference) {
        decl_ptr = g_static_descriptor_table->FindDescriptorSetDeclaration(decl.name);

        AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", decl.name.LookupString());
    }

    for (const Array<DescriptorDeclaration> &slot : decl_ptr->slots) {
        for (const DescriptorDeclaration &descriptor : slot) {
            const uint descriptor_index = decl_ptr->CalculateFlatIndex(descriptor.slot, descriptor.name);
            AssertThrow(descriptor_index != uint(-1));

            switch (descriptor.slot) {
            case DescriptorSlot::DESCRIPTOR_SLOT_SRV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE, descriptor_index, descriptor.count);
    
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_UAV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE_STORAGE, descriptor_index, descriptor.count);
                    
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_CBUFF:
                if (descriptor.is_dynamic) {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                } else {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SSBO:
                if (descriptor.is_dynamic) {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                } else {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE:
                AddElement(descriptor.name, DescriptorSetElementType::TLAS, descriptor_index, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER:
                AddElement(descriptor.name, DescriptorSetElementType::SAMPLER, descriptor_index, descriptor.count);

                break;
            default:
                AssertThrowMsg(false, "Invalid descriptor slot");
                break;
            }
        }
    }
}

template <>
DescriptorSet2Ref<Platform::VULKAN> DescriptorSetLayout<Platform::VULKAN>::CreateDescriptorSet() const
{
    DebugLog(LogType::Debug, "Create descriptor set with layout:\n");

    for (const auto &it : m_elements) {
        const String &name = it.first;
        const DescriptorSetLayoutElement &element = it.second;

        DebugLog(
            LogType::Debug,
            "\t%s: %u, binding: %u, count: %u\n",
            name.Data(),
            element.type,
            element.binding,
            element.count
        );
    }

    return MakeRenderObject<DescriptorSet2<Platform::VULKAN>>(*this);
}

// DescriptorSet2

DescriptorSet2<Platform::VULKAN>::DescriptorSet2(const DescriptorSetLayout<Platform::VULKAN> &layout)
    : m_layout(layout),
      m_vk_descriptor_set(VK_NULL_HANDLE)
{
    // Initial layout of elements
    for (auto &it : m_layout.GetElements()) {
        const String &name = it.first;
        const DescriptorSetLayoutElement &element = it.second;

        switch (element.type) {
        case DescriptorSetElementType::UNIFORM_BUFFER:          // fallthrough
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:  // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER:          // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:  // fallthrough
            PrefillElements<GPUBufferRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::IMAGE:                   // fallthrough
        case DescriptorSetElementType::IMAGE_STORAGE:           // fallthrough
            PrefillElements<ImageViewRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<SamplerRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::TLAS:
            PrefillElements<TLASRef<Platform::VULKAN>>(name, element.count);

            break;
        default:
            AssertThrowMsg(false, "Unhandled descriptor set element type in layout: %d", int(element.type));

            break;
        }
    }
}

DescriptorSet2<Platform::VULKAN>::~DescriptorSet2()
{
}

Result DescriptorSet2<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_vk_descriptor_set == VK_NULL_HANDLE);

    m_vk_layout_wrapper = device->GetDescriptorSetManager()->GetOrCreateVkDescriptorSetLayout(device, m_layout);

    Result result = Result::OK;

    HYPERION_PASS_ERRORS(
        device->GetDescriptorSetManager()->CreateDescriptorSet(device, m_vk_layout_wrapper, m_vk_descriptor_set),
        result
    );

    if (!result) {
        return result;
    }

    HYPERION_PASS_ERRORS(
        Update(device),
        result
    );

    return result;
}

Result DescriptorSet2<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_vk_descriptor_set != VK_NULL_HANDLE);

    device->GetDescriptorSetManager()->DestroyDescriptorSet(device, m_vk_descriptor_set);
    m_vk_descriptor_set = VK_NULL_HANDLE;

    // Release reference to layout
    m_vk_layout_wrapper.Reset();

    return Result::OK;
}

Result DescriptorSet2<Platform::VULKAN>::Update(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_vk_descriptor_set != VK_NULL_HANDLE);

    Array<VulkanDescriptorElementInfo> descriptor_element_infos;

    for (auto &it : m_elements) {
        const String &name = it.first;
        const DescriptorSetElement<Platform::VULKAN> &element = it.second;

        if (!element.IsDirty()) {
            continue;
        }

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.Data());

        for (uint i = element.dirty_range.GetStart(); i < element.dirty_range.GetEnd(); i++) {
            VulkanDescriptorElementInfo descriptor_element_info { };
            descriptor_element_info.binding = layout_element->binding;
            descriptor_element_info.index = i;
            descriptor_element_info.descriptor_type = ToVkDescriptorType(layout_element->type);

            const auto value_it = element.values.Find(i);

            if (value_it == element.values.End()) {
                continue;
            }

            if (value_it->second.Is<GPUBufferRef<Platform::VULKAN>>()) {
                const bool is_dynamic = layout_element->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
                    || layout_element->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

                const GPUBufferRef<Platform::VULKAN> &ref = value_it->second.Get<GPUBufferRef<Platform::VULKAN>>();
                AssertThrow(ref.IsValid());

                descriptor_element_info.buffer_info = VkDescriptorBufferInfo {
                    ref->buffer,
                    0,
                    element.buffer_size == 0
                        ? ref->size
                        : element.buffer_size
                };
            } else if (value_it->second.Is<ImageViewRef<Platform::VULKAN>>()) {
                const bool is_storage_image = layout_element->type == DescriptorSetElementType::IMAGE_STORAGE;

                const ImageViewRef<Platform::VULKAN> &ref = value_it->second.Get<ImageViewRef<Platform::VULKAN>>();
                AssertThrow(ref.IsValid());

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    VK_NULL_HANDLE,
                    ref->GetImageView(),
                    is_storage_image
                        ? VK_IMAGE_LAYOUT_GENERAL
                        : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            } else if (value_it->second.Is<SamplerRef<Platform::VULKAN>>()) {
                const SamplerRef<Platform::VULKAN> &ref = value_it->second.Get<SamplerRef<Platform::VULKAN>>();
                AssertThrow(ref.IsValid());

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    ref->GetSampler(),
                    VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED
                };
            } else if (value_it->second.Is<TLASRef<Platform::VULKAN>>()) {
                const TLASRef<Platform::VULKAN> &ref = value_it->second.Get<TLASRef<Platform::VULKAN>>();
                AssertThrow(ref.IsValid());

                descriptor_element_info.acceleration_structure_info = VkWriteDescriptorSetAccelerationStructureKHR {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    nullptr,
                    1,
                    &ref->GetAccelerationStructure()
                };
            } else {
                AssertThrowMsg(false, "Unhandled descriptor set element type: %d", int(layout_element->type));
            }

            descriptor_element_infos.PushBack(descriptor_element_info);
        }
    }

    Array<VkWriteDescriptorSet> vk_write_descriptor_sets;
    vk_write_descriptor_sets.Resize(descriptor_element_infos.Size());

    for (SizeType i = 0; i < vk_write_descriptor_sets.Size(); i++) {
        const VulkanDescriptorElementInfo &descriptor_element_info = descriptor_element_infos[i];

        VkWriteDescriptorSet &write = vk_write_descriptor_sets[i];
        
        write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = m_vk_descriptor_set;
        write.dstBinding = descriptor_element_info.binding;
        write.dstArrayElement = descriptor_element_info.index;
        write.descriptorCount = 1;
        write.descriptorType = descriptor_element_info.descriptor_type;

        if (descriptor_element_info.descriptor_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
            write.pNext = &descriptor_element_info.acceleration_structure_info;
        }
        
        write.pImageInfo = &descriptor_element_info.image_info;
        write.pBufferInfo = &descriptor_element_info.buffer_info;
    }

    vkUpdateDescriptorSets(
        device->GetDevice(),
        uint32(vk_write_descriptor_sets.Size()),
        vk_write_descriptor_sets.Data(),
        0,
        nullptr
    );

    for (auto &it : m_elements) {
        DescriptorSetElement<Platform::VULKAN> &element = it.second;

        element.dirty_range = { };
    }
    
    return Result::OK;
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, uint index, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, uint index, uint buffer_size, const GPUBufferRef<Platform::VULKAN> &ref)
{
    DescriptorSetElement<Platform::VULKAN> &element = SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);

    element.buffer_size = buffer_size;
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, uint index, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement<ImageViewRef<Platform::VULKAN>>(name, index, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, uint index, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement<SamplerRef<Platform::VULKAN>>(name, index, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

void DescriptorSet2<Platform::VULKAN>::SetElement(const String &name, uint index, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement<TLASRef<Platform::VULKAN>>(name, index, ref);
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const GraphicsPipelineRef<Platform::VULKAN> &pipeline, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        0,
        nullptr
    );
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const GraphicsPipelineRef<Platform::VULKAN> &pipeline, const Array<uint> &offsets, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        uint32(offsets.Size()),
        offsets.Data()
    );
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const ComputePipelineRef<Platform::VULKAN> &pipeline, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        0,
        nullptr
    );
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const ComputePipelineRef<Platform::VULKAN> &pipeline, const Array<uint> &offsets, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        uint32(offsets.Size()),
        offsets.Data()
    );
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const RaytracingPipelineRef<Platform::VULKAN> &pipeline, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        0,
        nullptr
    );
}

void DescriptorSet2<Platform::VULKAN>::Bind(const CommandBufferRef<Platform::VULKAN> &command_buffer, const RaytracingPipelineRef<Platform::VULKAN> &pipeline, const Array<uint> &offsets, uint bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->layout,
        bind_index,
        1,
        &m_vk_descriptor_set,
        uint32(offsets.Size()),
        offsets.Data()
    );
}

VkDescriptorSetLayout DescriptorSet2<Platform::VULKAN>::GetVkDescriptorSetLayout() const
{
    if (!m_vk_layout_wrapper) {
        return VK_NULL_HANDLE;
    }

    return m_vk_layout_wrapper->vk_layout;
}

// DescriptorSetManager

DescriptorSetManager<Platform::VULKAN>::DescriptorSetManager()
    : m_vk_descriptor_pool(VK_NULL_HANDLE)
{
}

DescriptorSetManager<Platform::VULKAN>::~DescriptorSetManager() = default;

Result DescriptorSetManager<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    static const Array<VkDescriptorPoolSize> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                    4096 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     4096 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              4096 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              32 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             32 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,     32 }
    };

    AssertThrow(m_vk_descriptor_pool == VK_NULL_HANDLE);

    VkDescriptorPoolCreateInfo pool_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = max_descriptor_sets;
    pool_info.poolSizeCount = uint32(pool_sizes.Size());
    pool_info.pPoolSizes = pool_sizes.Data();

    HYPERION_VK_CHECK(vkCreateDescriptorPool(
        device->GetDevice(),
        &pool_info,
        nullptr,
        &m_vk_descriptor_pool
    ));

    return Result::OK;
}

Result DescriptorSetManager<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    Result result = Result::OK;

    for (auto &it : m_vk_descriptor_set_layouts) {
        if (auto rc = it.second.Lock()) {
            HYPERION_PASS_ERRORS(
                rc->Destroy(device),
                result
            );
        }
    }

    m_vk_descriptor_set_layouts.Clear();

    if (m_vk_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            device->GetDevice(),
            m_vk_descriptor_pool,
            nullptr
        );

        m_vk_descriptor_pool = VK_NULL_HANDLE;
    }

    return result;
}

Result DescriptorSetManager<Platform::VULKAN>::CreateDescriptorSet(Device<Platform::VULKAN> *device, const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(layout != nullptr);
    AssertThrow(layout->vk_layout != VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = m_vk_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout->vk_layout;

    VkResult result = vkAllocateDescriptorSets(
        device->GetDevice(),
        &alloc_info,
        &out_vk_descriptor_set
    );

    if (result != VK_SUCCESS) {
        return { Result::RENDERER_ERR, "Failed to allocate descriptor set" };
    }

    return Result::OK;
}

Result DescriptorSetManager<Platform::VULKAN>::DestroyDescriptorSet(Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(vk_descriptor_set != VK_NULL_HANDLE);

    vkFreeDescriptorSets(
        device->GetDevice(),
        m_vk_descriptor_pool,
        1,
        &vk_descriptor_set
    );

    return Result::OK;
}

RC<VulkanDescriptorSetLayoutWrapper> DescriptorSetManager<Platform::VULKAN>::GetOrCreateVkDescriptorSetLayout(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
{
    const HashCode hash_code = layout.GetHashCode();

    auto it = m_vk_descriptor_set_layouts.Find(hash_code);

    if (it == m_vk_descriptor_set_layouts.End()) {
        RC<VulkanDescriptorSetLayoutWrapper> vk_descriptor_set_layout;
        vk_descriptor_set_layout.Reset(new VulkanDescriptorSetLayoutWrapper);
        
        HYPERION_ASSERT_RESULT(vk_descriptor_set_layout->Create(device, layout));

        m_vk_descriptor_set_layouts.Insert(hash_code, vk_descriptor_set_layout);

        return vk_descriptor_set_layout;
    }

    return it->second.Lock();
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
