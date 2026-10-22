/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Util/ThreadId.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Mutex.hpp>

namespace Hyperion {
namespace memory {

namespace {

thread_local ThreadAllocator* t_threadAllocator;

TArena<DynamicAllocator> g_leakyAllocator(ThreadAllocator::BlockSize, AF_THREAD_SAFE);

} // namespace


CORE_API ThreadAllocator& DefaultAllocatorInstanceHelper<ThreadAllocator>::operator()() const
{
    if (HYP_LIKELY(t_threadAllocator != nullptr))
    {
        return *t_threadAllocator;
    }

    ThreadBase* currentThread = CurrentThreadObject();
    AssertDebug(currentThread != nullptr, "ThreadAllocator used on thread that has no thread object!");

    if (currentThread != nullptr)
    {
        ThreadAllocator* threadAllocator = currentThread->GetThreadAllocator();
        AssertDebug(threadAllocator != nullptr, "ThreadAllocator used on thread that has none!");
        
        if (threadAllocator != nullptr)
        {
            return *(t_threadAllocator = threadAllocator);
        }
    }

    // if there is no thread object, or no ThreadAllocator for this thread -- fall back to a leaky allocator to not crash the process

    // Sue me
    return reinterpret_cast<ThreadAllocator&>(g_leakyAllocator);
}

} // namespace memory
} // namespace Hyperion
