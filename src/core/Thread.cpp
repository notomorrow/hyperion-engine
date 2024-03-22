#include <core/Thread.hpp>
#include <Threads.hpp>
#include <util/Defines.hpp>
#include <util/UTF8.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(HYP_UNIX)
#include <pthread.h>
#endif

namespace hyperion {

const ThreadID ThreadID::invalid = ThreadID { ~0u, HYP_NAME(InvalidThreadID) };

ThreadID ThreadID::Current()
{
    return Threads::CurrentThreadID();
}

ThreadID ThreadID::CreateDynamicThreadID(Name name)
{
    struct ThreadIDGenerator
    {
        AtomicVar<uint32> counter { 0 };

        uint32 Next()
        {
            return counter.Increment(1, MemoryOrder::SEQUENTIAL) + 1;
        }
    };

    static ThreadIDGenerator generator;

    return { generator.Next() << 16u, name };
}

bool ThreadID::IsDynamic() const
{
    return THREAD_DYNAMIC & value;
}

void SetCurrentThreadID(const ThreadID &thread_id)
{
    Threads::SetCurrentThreadID(thread_id);
}

void SetCurrentThreadPriority(ThreadPriorityValue priority)
{
    Threads::SetCurrentThreadPriority(priority);
}


} // namespace hyperion