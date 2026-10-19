/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>
#include <Framework/EngineGlobals.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT int32 SimThread_IsOnIt()
    {
        return IsOnThread(g_simThread);
    }

    HYP_EXPORT void SimThread_PostTask(void (*pTaskFunc)(void*), void* context)
    {
        Assert(pTaskFunc != nullptr);

        if (IsOnThread(g_simThread))
        {
            // Execute immediately if already on the sim thread
            pTaskFunc(context);
            return;
        }

        g_simThreadInstance->GetScheduler().Enqueue([pTaskFunc, context]()
            {
                pTaskFunc(context);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
} // extern "C"
