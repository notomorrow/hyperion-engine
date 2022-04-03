#ifndef HYPERION_V2_BINDLESS_H
#define HYPERION_V2_BINDLESS_H

#include "texture.h"
#include "util.h"

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

    /*! \brief Apply changes to the bindless descriptor set corresponding to the given frame index value.
     * Do not call this with the index of a frame that is still using resources.
     */
    void ApplyUpdates(Engine *engine, uint32_t frame_index);

    /*! \brief Add a texture to the bindless descriptor set. */
    void AddResource(Texture *texture);
    /*! \brief Remove the given texture from the bindless descriptor set. */
    void RemoveResource(Texture *texture);
    /*! \brief Mark a resource as having changed, to be queued for update. */
    void MarkResourceChanged(Texture *texture);

private:
    ObjectIdMap<Texture, size_t> m_texture_sub_descriptors;
    std::array<DescriptorSet *, Swapchain::max_frames_in_flight> m_descriptor_sets;
};

} // namespace hyperion::v2

#endif