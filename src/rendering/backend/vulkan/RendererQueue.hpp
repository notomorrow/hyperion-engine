#ifndef HYPERION_RENDERER_QUEUE_H
#define HYPERION_RENDERER_QUEUE_H

#include <core/lib/FixedArray.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

struct DeviceQueue
{
    UInt family;
    VkQueue queue;
    FixedArray<VkCommandPool, 8> command_pools;
};

} // namespace renderer
} // namespace hyperion

#endif
