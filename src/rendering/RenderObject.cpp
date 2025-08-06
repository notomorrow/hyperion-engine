/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderObject.hpp>
#include <rendering/RenderCommandBuffer.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region RenderObjectContainerBase

RenderObjectContainerBase::RenderObjectContainerBase(ANSIStringView renderObjectTypeName)
{
}

#pragma endregion RenderObjectContainerBase

#pragma region RenderObjectDeleter

FixedArray<DeletionQueueBase*, RenderObjectDeleter::maxQueues + 1> RenderObjectDeleter::s_queues = {};
AtomicVar<uint16> RenderObjectDeleter::s_queueIndex = { 0 };

void RenderObjectDeleter::Initialize()
{
    // Command buffer should be deleted first so that no
    // buffers that will be deleted are used in the command buffers
    (void)GetQueue<CommandBufferBase>();
}

void RenderObjectDeleter::Iterate()
{
    HYP_NAMED_SCOPE("Destroy enqueued rendering resources");

    DeletionQueueBase** queue = s_queues.Data();

    while (*queue)
    {
        (*queue)->Iterate();
        ++queue;
    }
}

void RenderObjectDeleter::RemoveAllNow(bool force)
{
    HYP_NAMED_SCOPE("Force delete all rendering resources");

    FixedArray<AtomicVar<int32>*, s_queues.Size()> queueNumItems {};

    { // init atomic vars
        DeletionQueueBase** queue = s_queues.Data();
        for (uint32 queueIndex = 0; *queue; ++queueIndex, ++queue)
        {
            queueNumItems[queueIndex] = &(*queue)->numItems;
        }
    }

    // Loop until all queues are empty
    while (AnyOf(queueNumItems, [](AtomicVar<int32>* count)
        {
            return count != nullptr && count->Get(MemoryOrder::ACQUIRE) > 0;
        }))
    {
        for (DeletionQueueBase** queue = s_queues.Data(); *queue; ++queue)
        {
            (*queue)->RemoveAllNow(force);
        }
    }
}

#pragma endregion RenderObjectDeleter

} // namespace hyperion