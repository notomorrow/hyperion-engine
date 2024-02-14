#ifndef HYPERION_V2_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET2_HPP
#define HYPERION_V2_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET2_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/Mutex.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <util/Defines.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class GPUBuffer;

template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

struct VulkanDescriptorSetLayoutWrapper;

template <>
class DescriptorSet2<Platform::VULKAN>
{
public:
    DescriptorSet2(const DescriptorSetLayout<Platform::VULKAN> &layout);
    DescriptorSet2(const DescriptorSet2 &other)                 = delete;
    DescriptorSet2 &operator=(const DescriptorSet2 &other)      = delete;
    DescriptorSet2(DescriptorSet2 &&other) noexcept             = delete;
    DescriptorSet2 &operator=(DescriptorSet2 &&other) noexcept  = delete;
    ~DescriptorSet2();

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    Result Update(Device<Platform::VULKAN> *device);
    
    void SetElement(const String &name, const GPUBufferRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const GPUBufferRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, uint buffer_size, const GPUBufferRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const ImageViewRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const ImageViewRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const SamplerRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const SamplerRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const TLASRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const TLASRef<Platform::VULKAN> &ref);

private:
    template <class T>
    DescriptorSetElement<Platform::VULKAN> &SetElement(const String &name, uint index, const T &ref)
    {
        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.Data());

        // Type check
        static const uint32 mask = DescriptorSetElementTypeInfo<typename T::Type>::mask;
        AssertThrowMsg(mask & (1u << uint32(layout_element->type)), "Layout type for %s does not match given type", name.Data());

        // Range check
        AssertThrowMsg(
            index < layout_element->count,
            "Index %u out of range for element %s with count %u",
            index,
            name.Data(),
            layout_element->count
        );

        // Buffer type check, to make sure the buffer type is allowed for the given element
        if constexpr (std::is_same_v<typename T::Type, GPUBuffer<Platform::VULKAN>>) {
            if (ref != nullptr) {
                const GPUBufferType buffer_type = ref->GetBufferType();

                AssertThrowMsg(
                    (descriptor_set_element_type_to_buffer_type[uint(layout_element->type)] & (1u << uint(buffer_type))),
                    "Buffer type %u is not in the allowed types for element %s",
                    uint(buffer_type),
                    name.Data()
                );
            }
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            it = m_elements.Insert({
                name,
                DescriptorSetElement<Platform::VULKAN> { }
            }).first;
        }

        DescriptorSetElement<Platform::VULKAN> &element = it->second;

        auto element_it = element.values.Find(index);

        if (element_it == element.values.End()) {
            element_it = element.values.Insert({
                index,
                NormalizedType<T>(ref)
            }).first;
        } else {
            T *current_value = element_it->second.TryGet<T>();

            if (current_value) {
                SafeRelease(std::move(*current_value));
            }

            element_it->second.Set<NormalizedType<T>>(ref);
        }

        // Mark the range as dirty so that it will be updated in the next update
        element.dirty_range |= { index, index + 1 };

        return element;
    }

    template <class T>
    void PrefillElements(const String &name, uint count)
    {
        bool is_bindless = false;

        if (count == ~0u) {
            count = max_bindless_resources;
            is_bindless = true;
        }

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.Data());

        if (is_bindless) {
            AssertThrowMsg(
                layout_element->IsBindless(),
                "-1 given as count to prefill elements, yet %s is not specified as bindless in layout",
                name.Data()
            );
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            it = m_elements.Insert({
                name,
                DescriptorSetElement<Platform::VULKAN> { }
            }).first;
        }

        DescriptorSetElement<Platform::VULKAN> &element = it->second;
        
        element.values.Clear();
        element.values.Reserve(count);

        for (uint i = 0; i < count; i++) {
            element.values.Set(i, T { });
        }

        element.dirty_range = { 0, count };
    }

    DescriptorSetLayout<Platform::VULKAN>                       m_layout;
    ArrayMap<String, DescriptorSetElement<Platform::VULKAN>>    m_elements;

    VkDescriptorSet                                             m_vk_descriptor_set;
};

template <>
class DescriptorSetManager<Platform::VULKAN>
{
public:
    static constexpr uint max_descriptor_sets = 4096;

    DescriptorSetManager();
    DescriptorSetManager(const DescriptorSetManager &other)                 = delete;
    DescriptorSetManager &operator=(const DescriptorSetManager &other)      = delete;
    DescriptorSetManager(DescriptorSetManager &&other) noexcept             = delete;
    DescriptorSetManager &operator=(DescriptorSetManager &&other) noexcept  = delete;
    ~DescriptorSetManager();

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    Result CreateDescriptorSet(Device<Platform::VULKAN> *device, const VulkanDescriptorSetLayoutWrapper *layout, VkDescriptorSet &out_vk_descriptor_set);
    Result DestroyDescriptorSet(Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set);

    RC<VulkanDescriptorSetLayoutWrapper> GetOrCreateVkDescriptorSetLayout(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout);

private:
    HashMap<HashCode, RC<VulkanDescriptorSetLayoutWrapper>> m_vk_descriptor_set_layouts;

    VkDescriptorPool                                        m_vk_descriptor_pool;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif