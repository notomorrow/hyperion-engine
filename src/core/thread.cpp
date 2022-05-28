#include "thread.h"

namespace hyperion::v2 {

void SetThreadId(const ThreadId &id)
{
    current_thread_id = id;
}

} // namespace hyperion::v2