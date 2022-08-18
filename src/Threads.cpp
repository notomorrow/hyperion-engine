#include "Threads.hpp"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadId> Threads::thread_ids {
    decltype(thread_ids)::KeyValuePair { THREAD_MAIN,    ThreadId { static_cast<UInt>(THREAD_MAIN),    "MainThread" } },
    // decltype(thread_ids)::Pair { THREAD_RENDER   ThreadId { static_cast<UInt>(THREAD_RENDER),  "RenderThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_GAME,    ThreadId { static_cast<UInt>(THREAD_GAME),    "GameThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TERRAIN, ThreadId { static_cast<UInt>(THREAD_TERRAIN), "TerrainGenerationThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_0,  ThreadId { static_cast<UInt>(THREAD_TASK_0), "TaskThread0" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_1,  ThreadId { static_cast<UInt>(THREAD_TASK_1), "TaskThread1" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_2,  ThreadId { static_cast<UInt>(THREAD_TASK_2), "TaskThread2" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_3,  ThreadId { static_cast<UInt>(THREAD_TASK_3), "TaskThread3" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_4,  ThreadId { static_cast<UInt>(THREAD_TASK_4), "TaskThread4" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_5,  ThreadId { static_cast<UInt>(THREAD_TASK_5), "TaskThread5" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_6,  ThreadId { static_cast<UInt>(THREAD_TASK_6), "TaskThread6" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_7,  ThreadId { static_cast<UInt>(THREAD_TASK_7), "TaskThread7" } },
};

#if HYP_ENABLE_THREAD_ASSERTION
thread_local ThreadId current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif

void Threads::AssertOnThread(ThreadMask mask)
{
#if HYP_ENABLE_THREAD_ASSERTION
    const auto &current = current_thread_id;

    AssertThrowMsg(
        (mask & current.value),
        "Expected current thread to be in mask %u, but got %u (%s)",
        mask,
        current.name.CString(),
        current.value
    );
#endif
}

bool Threads::IsOnThread(ThreadMask mask)
{
#if HYP_ENABLE_THREAD_ASSERTION
    if (mask & current_thread_id.value) {
        return true;
    }

#else
    DebugLog(
        LogType::Error,
        "IsOnThread() called but thread IDs are currently disabled!\n"
    );
#endif

    return false;
}

ThreadId Threads::GetThreadId(ThreadName thread_name)
{
    return thread_ids.At(thread_name);
}

} // namespace hyperion::v2