/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/MathUtil.hpp>
#include <Types.hpp>
#include <core/threading/TaskSystem.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {

enum class IterationResult : uint8
{
    CONTINUE = 0,
    STOP
};

/*! \brief Execute a callback for each item in the container.
 *  Callback is called with the item as an argument and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param callback The function to call for each item.
 */
template <class Container, class Callback>
static inline void ForEach(Container&& container, Callback&& callback)
{
    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP)
        {
            break;
        }
    }
}

/*! \brief Execute a callback for each item in the container, locking the mutex before iterating.
 *  Callback is called with the item as an argument and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param mutex The mutex to lock before iterating.
 *  \param callback The function to call for each item.
 */
template <class Container, class Mutex, class Callback>
static inline void ForEach(Container&& container, Mutex& mutex, Callback&& callback)
{
    typename Mutex::Guard guard(mutex);

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP)
        {
            break;
        }
    }
}

/*! \brief Execute a lambda for each item in the container, in \ref{numBatches} batches.
 *  Container must be a contiguous container.
 *  Callback is called with the item, the index of the item, and the batch index and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param numBatches The number of batches to split the container into.
 *  \param callback The function to call for each item.
 */
template <class Container, class Callback>
static inline void ForEachInBatches(Container&& container, uint32 numBatches, Callback&& callback)
{
    static_assert(NormalizedType<Container>::isContiguous, "Container must be contiguous to use ForEachInBatches");

    const uint32 numItems = container.Size();
    const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

    auto* dataPtr = container.Data();

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        const uint32 offsetIndex = batchIndex * itemsPerBatch;
        const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

        for (uint32 i = offsetIndex; i < maxIndex; ++i)
        {
            IterationResult result = callback(*(dataPtr + i), i, batchIndex);

            if (result == IterationResult::STOP)
            {
                break;
            }
        }
    }
}

} // namespace hyperion
