#ifndef HYPERION_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_RENDERER_DESCRIPTOR_SET_H

#include "renderer_descriptor.h"
#include "renderer_result.h"

#include <vector>
#include <memory>

namespace hyperion {
namespace renderer {
class Device;
class DescriptorPool;
class DescriptorSet {
    friend class Descriptor;
public:
    enum Index {
        DESCRIPTOR_SET_INDEX_GLOBALS  = 0, /* per frame */
        DESCRIPTOR_SET_INDEX_PASS     = 1, /* per render pass */
        DESCRIPTOR_SET_INDEX_SCENE    = 2, /* per scene */
        DESCRIPTOR_SET_INDEX_OBJECT   = 3  /* per object */
    };

    DescriptorSet();
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    ~DescriptorSet();

    inline Descriptor::State GetState() const { return m_state; }

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
    Descriptor::State m_state;
};

} // namespace renderer
} // namespace hyperion

#endif