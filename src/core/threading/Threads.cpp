/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Threads.hpp>

namespace hyperion {
namespace threading {

const FlatMap<ThreadName, ThreadID> Threads::thread_ids = {
    { THREAD_MAIN,      ThreadID { uint(THREAD_MAIN),       HYP_NAME_UNSAFE(MainThread) } },
    { THREAD_GAME,      ThreadID { uint(THREAD_GAME),       HYP_NAME_UNSAFE(GameThread) } },
    { THREAD_TERRAIN,   ThreadID { uint(THREAD_TERRAIN),    HYP_NAME_UNSAFE(TerrainGenerationThread) } },
    { THREAD_TASK_0,    ThreadID { uint(THREAD_TASK_0),     HYP_NAME_UNSAFE(TaskThread0) } },
    { THREAD_TASK_1,    ThreadID { uint(THREAD_TASK_1),     HYP_NAME_UNSAFE(TaskThread1) } },
    { THREAD_TASK_2,    ThreadID { uint(THREAD_TASK_2),     HYP_NAME_UNSAFE(TaskThread2) } },
    { THREAD_TASK_3,    ThreadID { uint(THREAD_TASK_3),     HYP_NAME_UNSAFE(TaskThread3) } },
    { THREAD_TASK_4,    ThreadID { uint(THREAD_TASK_4),     HYP_NAME_UNSAFE(TaskThread4) } },
    { THREAD_TASK_5,    ThreadID { uint(THREAD_TASK_5),     HYP_NAME_UNSAFE(TaskThread5) } },
    { THREAD_TASK_6,    ThreadID { uint(THREAD_TASK_6),     HYP_NAME_UNSAFE(TaskThread6) } },
    { THREAD_TASK_7,    ThreadID { uint(THREAD_TASK_7),     HYP_NAME_UNSAFE(TaskThread7) } },
    { THREAD_TASK_8,    ThreadID { uint(THREAD_TASK_8),     HYP_NAME_UNSAFE(TaskThread8) } },
    { THREAD_TASK_9,    ThreadID { uint(THREAD_TASK_9),     HYP_NAME_UNSAFE(TaskThread9) } },
    { THREAD_TASK_10,   ThreadID { uint(THREAD_TASK_10),    HYP_NAME_UNSAFE(TaskThread10) } }
};

#ifdef HYP_ENABLE_THREAD_ID
thread_local ThreadID current_thread_id = ThreadID { uint(THREAD_MAIN), HYP_NAME_UNSAFE(MainThread) };
#else
static const ThreadID current_thread_id = Threads::thread_ids.At(THREAD_MAIN);
#endif

void Threads::SetCurrentThreadID(ThreadID id)
{
#ifdef HYP_ENABLE_THREAD_ID
    current_thread_id = id;
#endif
    
    DebugLog(LogType::Debug, "SetCurrentThreadID() %u\n", id.value);

#ifdef HYP_WINDOWS
    HRESULT set_thread_result = SetThreadDescription(
        GetCurrentThread(),
        &HYP_UTF8_TOWIDE(id.name.LookupString())[0]
    );

    if (FAILED(set_thread_result)) {
        DebugLog(
            LogType::Warn,
            "Failed to set Win32 thread name for thread %s\n",
            id.name.LookupString()
        );
    }
#elif defined(HYP_MACOS)
    pthread_setname_np(id.name.LookupString());
#elif defined(HYP_LINUX)
    pthread_setname_np(pthread_self(), id.name.LookupString());
#endif
}

void Threads::AssertOnThread(ThreadMask mask, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        (mask & current.value),
        "Expected current thread to be in mask %u, but got %u (%s). Message: %s",
        mask,
        current.value,
        current.name.LookupString(),
        message ? message : "(no message)"
    );

#else
    DebugLog(
        LogType::Error,
        "AssertOnThread() called but thread IDs are currently disabled!\n"
    );
#endif
#endif
}

void Threads::AssertOnThread(const ThreadID &thread_id, const char *message)
{
#ifdef HYP_ENABLE_THREAD_ASSERTIONS
#ifdef HYP_ENABLE_THREAD_ID
    const auto &current = current_thread_id;

    AssertThrowMsg(
        thread_id == current,
        "Expected current thread to be %u (%s), but got %u (%s). Message: %s",
        thread_id.value,
        thread_id.name.LookupString(),
        current.value,
        current.name.LookupString(),
        message ? message : "(no message)"
    );
#else
    DebugLog(
        LogType::Error,
        "AssertOnThread() called but thread IDs are currently disabled!\n"
    );
#endif
#endif
}

bool Threads::IsThreadInMask(const ThreadID &thread_id, ThreadMask mask)
{
    if (mask & thread_id.value) {
        return true;
    }

    return false;
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

bool Threads::IsOnThread(const ThreadID &thread_id)
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

const ThreadID &Threads::GetThreadID(ThreadName thread_name)
{
    return thread_ids.At(thread_name);
}

const ThreadID &Threads::CurrentThreadID()
{
    return current_thread_id;
}

ThreadType Threads::GetThreadType()
{
    const uint thread_id = Threads::CurrentThreadID().value;

    return thread_id == THREAD_GAME ? THREAD_TYPE_GAME
        : (thread_id == THREAD_RENDER ? THREAD_TYPE_RENDER : THREAD_TYPE_INVALID);
}

void Threads::SetCurrentThreadPriority(ThreadPriorityValue priority)
{
#ifdef HYP_WINDOWS
    int win_priority = THREAD_PRIORITY_NORMAL;

    switch (priority) {
    case ThreadPriorityValue::LOWEST:
        win_priority = THREAD_PRIORITY_LOWEST;
        break;

    case ThreadPriorityValue::LOW:
        win_priority = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case ThreadPriorityValue::NORMAL:
        win_priority = THREAD_PRIORITY_NORMAL;
        break;

    case ThreadPriorityValue::HIGH:
        win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case ThreadPriorityValue::HIGHEST:
        win_priority = THREAD_PRIORITY_HIGHEST;
        break;
    }

    SetThreadPriority(GetCurrentThread(), win_priority);
#elif defined(HYP_LINUX) || defined(HYP_APPLE)
    int policy = SCHED_OTHER;
    struct sched_param param;

    switch (priority) {
    case ThreadPriorityValue::LOWEST:
        param.sched_priority = sched_get_priority_min(policy);
        break;

    case ThreadPriorityValue::LOW:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 4;
        break;

    case ThreadPriorityValue::NORMAL:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) / 2;
        break;

    case ThreadPriorityValue::HIGH:
        param.sched_priority = (sched_get_priority_min(policy) + sched_get_priority_max(policy)) * 3 / 4;
        break;

    case ThreadPriorityValue::HIGHEST:
        param.sched_priority = sched_get_priority_max(policy);
        break;
    }

    pthread_setschedparam(pthread_self(), policy, &param);
#endif
}

SizeType Threads::NumCores()
{
    return std::thread::hardware_concurrency();
}

void Threads::Sleep(uint32 milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace threading
} // namespace hyperion