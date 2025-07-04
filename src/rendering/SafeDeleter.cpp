/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RemoveTextureFromBindlessStorage)
    : RenderCommand
{
    ObjId<Texture> id;

    RENDER_COMMAND(RemoveTextureFromBindlessStorage)(ObjId<Texture> id)
        : id(id)
    {
    }

    virtual ~RENDER_COMMAND(RemoveTextureFromBindlessStorage)() override = default;

    virtual RendererResult operator()() override
    {
        g_renderGlobalState->bindlessStorage->RemoveResource(id);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region DeletionEntryBase

bool DeletionEntryBase::DecrementCycle()
{
    if (m_cyclesRemaining == 0)
    {
        return true;
    }

    --m_cyclesRemaining;

    return m_cyclesRemaining == 0;
}

bool DeletionEntryBase::PerformDeletion(bool force)
{
    if (force)
    {
        m_cyclesRemaining = 0;
    }

    if (m_cyclesRemaining != 0)
    {
        return false;
    }

    PerformDeletion_Internal();

    return true;
}

#pragma endregion DeletionEntryBase

#pragma region SafeDeleter

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (int32 numDeletionEntries = m_numDeletionEntries.Get(MemoryOrder::ACQUIRE))
    {
        Array<UniquePtr<DeletionEntryBase>> deletionEntries;

        { // Critical section
            Mutex::Guard guard(m_mutex);

            CollectAllEnqueuedItems(deletionEntries);
        }

        for (auto it = deletionEntries.Begin(); it != deletionEntries.End(); ++it)
        {
            Assert((*it)->PerformDeletion());

            m_numDeletionEntries.Decrement(1, MemoryOrder::RELEASE);
        }
    }
}

void SafeDeleter::ForceDeleteAll()
{
    while (int32 numDeletionEntries = m_numDeletionEntries.Get(MemoryOrder::ACQUIRE))
    {
        Array<UniquePtr<DeletionEntryBase>> deletionEntries;

        { // Critical section
            Mutex::Guard guard(m_mutex);

            CollectAllEnqueuedItems(deletionEntries);
        }

        for (auto it = deletionEntries.Begin(); it != deletionEntries.End();)
        {
            Assert((*it)->PerformDeletion(true /* force */));

            it = deletionEntries.Erase(it);

            m_numDeletionEntries.Decrement(1, MemoryOrder::RELEASE);
        }
    }
}

bool SafeDeleter::CollectAllEnqueuedItems(Array<UniquePtr<DeletionEntryBase>>& outEntries)
{
    for (auto it = m_deletionEntries.Begin(); it != m_deletionEntries.End();)
    {
        auto& entry = *it;

        if (entry->DecrementCycle())
        {
            outEntries.PushBack(std::move(*it));

            it = m_deletionEntries.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    return m_deletionEntries.Empty();
}

#pragma endregion SafeDeleter

} // namespace hyperion