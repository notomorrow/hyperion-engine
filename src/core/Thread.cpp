#include "Thread.hpp"

namespace hyperion::v2 {

void SetThreadID(const ThreadID &id)
{
#ifdef HYP_ENABLE_THREAD_ID
    current_thread_id = id;
#endif
}

} // namespace hyperion::v2