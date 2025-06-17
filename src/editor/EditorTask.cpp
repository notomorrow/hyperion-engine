/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorTask.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region TickableEditorTask

TickableEditorTask::TickableEditorTask()
    : m_isCommitted(false)
{
}

void TickableEditorTask::Commit()
{
    ThreadBase* gameThread = Threads::GetThread(g_gameThread);
    AssertThrow(gameThread != nullptr);

    m_task = gameThread->GetScheduler().Enqueue([weakThis = WeakHandleFromThis()]()
        {
            if (Handle<TickableEditorTask> task = weakThis.Lock())
            {
                task->m_isCommitted.Set(true, MemoryOrder::RELEASE);

                task->Process();
            }
            else
            {
                HYP_LOG(Editor, Warning, "EditorTask was destroyed before it could be processed");
            }
        });
}

void TickableEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!Threads::IsOnThread(g_gameThread))
        {
            HYP_LOG(Editor, Info, "Awaiting TickableEditorTask completion");

            m_task.Await();
        }
        else
        {
            HYP_LOG(Editor, Info, "Executing TickableEditorTask inline");

            AssertThrow(m_task.Cancel());

            auto* promise = m_task.Promise();

            promise->Fulfill();
        }

        OnCancel();
    }

    m_isCommitted.Set(false, MemoryOrder::RELEASE);
}

bool TickableEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion TickableEditorTask

#pragma region LongRunningEditorTask

LongRunningEditorTask::LongRunningEditorTask()
    : m_isCommitted(false)
{
}

void LongRunningEditorTask::Commit()
{
    m_task = TaskSystem::GetInstance().Enqueue([this]()
        {
            m_isCommitted.Set(true, MemoryOrder::RELEASE);

            Process();
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);
}

void LongRunningEditorTask::Cancel_Impl()
{
    if (m_task.IsValid() && !m_task.IsCompleted())
    {
        if (!m_task.Cancel())
        {
            HYP_LOG(Editor, Warning, "Failed to cancel task, awaiting...");

            m_task.Await();
        }

        OnCancel();
    }

    m_isCommitted.Set(false, MemoryOrder::RELEASE);
}

bool LongRunningEditorTask::IsCompleted_Impl() const
{
    return m_task.IsCompleted();
}

#pragma endregion LongRunningEditorTask

} // namespace hyperion