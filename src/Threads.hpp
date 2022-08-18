#ifndef HYPERION_V2_THREADS_H
#define HYPERION_V2_THREADS_H

#include <core/lib/FlatMap.hpp>
#include <core/Thread.hpp>

#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

using ThreadMask = UInt;

enum ThreadName : ThreadMask {
    THREAD_MAIN    = 0x01,
    THREAD_INPUT   = 0x01,
    THREAD_RENDER  = 0x01, // for now
    THREAD_GAME    = 0x04,
    THREAD_TERRAIN = 0x08,

    THREAD_TASK_0  = 0x10,
    THREAD_TASK_1  = 0x20,
    THREAD_TASK_2  = 0x30,
    THREAD_TASK_3  = 0x40,
    THREAD_TASK_4  = 0x50,
    THREAD_TASK_5  = 0x60,
    THREAD_TASK_6  = 0x70,
    THREAD_TASK_7  = 0x80,

    THREAD_TASK    = 0xF0
};

class Threads {
public:
    static const FlatMap<ThreadName, ThreadId> thread_ids;

    static void AssertOnThread(ThreadMask mask);
    static bool IsOnThread(ThreadMask mask);

    static ThreadId GetThreadId(ThreadName thread_name);
};

} // namespace hyperion::v2

#endif