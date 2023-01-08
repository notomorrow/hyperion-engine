#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

FixedArray<RenderObjectDeleter::DeletionQueueBase *, RenderObjectDeleter::max_queues + 1> RenderObjectDeleter::queues = { };
std::atomic_uint16_t RenderObjectDeleter::queue_index = { 0 };

} // namespace hyperion