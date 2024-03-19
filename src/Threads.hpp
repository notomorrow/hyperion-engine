#ifndef HYPERION_V2_THREADS_H
#define HYPERION_V2_THREADS_H

#include <core/lib/FlatMap.hpp>
#include <core/Thread.hpp>

#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion {

using ThreadMask = uint32;

enum ThreadName : ThreadMask
{
    THREAD_MAIN     = 0x01u,
    THREAD_RENDER   = THREAD_MAIN, // for now
    THREAD_GAME     = 0x04u,
    THREAD_INPUT    = THREAD_MAIN, // for now
    THREAD_TERRAIN  = 0x08u,

    THREAD_TASK_0   = 0x10u,
    THREAD_TASK_1   = 0x20u,
    THREAD_TASK_2   = 0x40u,
    THREAD_TASK_3   = 0x80u,
    THREAD_TASK_4   = 0x100u,
    THREAD_TASK_5   = 0x200u,
    THREAD_TASK_6   = 0x400u,
    THREAD_TASK_7   = 0x800u,
    THREAD_TASK_8   = 0x1000u,
    THREAD_TASK_9   = 0x2000u,
    THREAD_TASK_10  = 0x4000u,

    THREAD_TASK     = 0x7ff0u, // all task threads or'd together

    THREAD_PHYSICS  = THREAD_GAME, // for now

    THREAD_STATIC   = 0xFFFFu,
    THREAD_DYNAMIC  = 0xFFFFu << 16u
};

// Used for having 1 value of something per thread,
// e.g `uint counter[THREAD_TYPE_MAX]` and selecting the value
// based on the current thread.
enum ThreadType : uint32
{
    THREAD_TYPE_INVALID = uint32(-1),
    THREAD_TYPE_GAME    = 0,
    THREAD_TYPE_RENDER  = 1,
    THREAD_TYPE_MAX
};

class Threads
{
public:
    static const FlatMap<ThreadName, ThreadID> thread_ids;

    static void AssertOnThread(ThreadMask mask, const char *message = nullptr);
    static void AssertOnThread(const ThreadID &thread_id, const char *message = nullptr);
    static bool IsThreadInMask(const ThreadID &thread_id, ThreadMask mask);
    static bool IsOnThread(ThreadMask mask);
    static bool IsOnThread(const ThreadID &thread_id);
    static const ThreadID &GetThreadID(ThreadName thread_name);
    static const ThreadID &CurrentThreadID();
    static void SetThreadID(const ThreadID &thread_id);
    static void SetCurrentThreadPriority(ThreadPriorityValue priority);

    static ThreadType GetThreadType();

    static SizeType NumCores();

    static void Sleep(uint32 milliseconds);
};

} // namespace hyperion

#endif