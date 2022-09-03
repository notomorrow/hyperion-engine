#include "Threads.hpp"

namespace hyperion::v2 {

const FlatMap<ThreadName, ThreadID> Threads::thread_ids = {
    decltype(thread_ids)::KeyValuePair { THREAD_MAIN,    ThreadID { static_cast<UInt>(THREAD_MAIN),    "MainThread" } },
    // decltype(thread_ids)::Pair { THREAD_RENDER   ThreadID { static_cast<UInt>(THREAD_RENDER),  "RenderThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_GAME,    ThreadID { static_cast<UInt>(THREAD_GAME),    "GameThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TERRAIN, ThreadID { static_cast<UInt>(THREAD_TERRAIN), "TerrainGenerationThread" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_0,  ThreadID { static_cast<UInt>(THREAD_TASK_0), "TaskThread0" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_1,  ThreadID { static_cast<UInt>(THREAD_TASK_1), "TaskThread1" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_2,  ThreadID { static_cast<UInt>(THREAD_TASK_2), "TaskThread2" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_3,  ThreadID { static_cast<UInt>(THREAD_TASK_3), "TaskThread3" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_4,  ThreadID { static_cast<UInt>(THREAD_TASK_4), "TaskThread4" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_5,  ThreadID { static_cast<UInt>(THREAD_TASK_5), "TaskThread5" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_6,  ThreadID { static_cast<UInt>(THREAD_TASK_6), "TaskThread6" } },
    decltype(thread_ids)::KeyValuePair { THREAD_TASK_7,  ThreadID { static_cast<UInt>(THREAD_TASK_7), "TaskThread7" } },
};

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif

void Threads::AssertOnThread(ThreadMask mask)
{
#ifdef HYP_ENABLE_THREAD_ID
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
#ifdef HYP_ENABLE_THREAD_ID
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

bool Threads::IsOnThread(ThreadID thread_id)
{
#ifdef HYP_ENABLE_THREAD_ID
    if (thread_id == current_thread_id) {
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

ThreadID Threads::GetThreadID(ThreadName thread_name)
{
    return thread_ids.At(thread_name);
}

ThreadID Threads::CurrentThreadID()
{
    return current_thread_id;
}


} // namespace hyperion::v2