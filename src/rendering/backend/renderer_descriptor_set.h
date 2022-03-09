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
public:
    DescriptorSet();
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    ~DescriptorSet();

    // return this
    DescriptorSet &AddDescriptor(std::unique_ptr<Descriptor> &&);

    inline Descriptor *GetDescriptor(size_t index) { return m_descriptors[index].get(); }
    inline const Descriptor *GetDescriptor(size_t index) const { return m_descriptors[index].get(); }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

    VkDescriptorSet m_set;

private:
    std::vector<std::unique_ptr<Descriptor>> m_descriptors;

};

} // namespace renderer
} // namespace hyperion

#endif