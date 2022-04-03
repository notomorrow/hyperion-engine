#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include "texture.h"

#include <rendering/backend/renderer_descriptor_set.h>
#include <rendering/backend/renderer_swapchain.h>

#include <queue>
#include <array>

namespace hyperion::v2 {

class Engine;

using renderer::DescriptorSet;
using renderer::Swapchain;

class BindlessStorage {
public:
    BindlessStorage();
    BindlessStorage(const BindlessStorage &other) = delete;
    BindlessStorage &operator=(const BindlessStorage &other) = delete;
    ~BindlessStorage();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void ApplyUpdates(Engine *engine, uint32_t frame_index);

    void AddResource(Texture *texture);
    void RemoveResource(Texture *texture);
    void MarkResourceChanged(Texture *texture);

private:
    std::queue<size_t> m_update_queue;
    std::array<DescriptorSet *, Swapchain::max_frames_in_flight> m_descriptor_sets;
};

} // namespace hyperion::v2

#endif