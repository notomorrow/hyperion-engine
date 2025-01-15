/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_FOREACH_HPP
#define HYPERION_UTIL_FOREACH_HPP

#include <core/containers/Array.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>
#include <core/threading/TaskSystem.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {


/*! \brief Execute a lambda for each item in the container, in \ref{num_batches} batches.
 *  Container must be a contiguous container.
 *  Lambda is called with the item, the index of the item, and the batch index.
 *
 *  \param container The container to iterate over.
 *  \param num_batches The number of batches to split the container into.
 *  \param lambda The lambda to call for each item.
 */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ForEachInBatches(Container &container, uint32 num_batches, Lambda &&lambda)
{
    static_assert(Container::is_contiguous, "Container must be contiguous to use ForEachInBatches");

    const uint32 num_items = container.Size();
    const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

    auto *data_ptr = container.Data();

    for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
        const uint32 offset_index = batch_index * items_per_batch;
        const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

        for (uint32 i = offset_index; i < max_index; ++i) {
            lambda(*(data_ptr + i), i, batch_index);
        }
    }
}

/*! \brief Call a lambda for each permutation of items in the container.
 *  Container must be a contiguous container.
 *  Lambda is called with an Array of indices into the container.
 *
 *  \param container The container to iterate over.
 *  \param lambda The lambda to call for each permutation.
 */
template <class Container, class Lambda>
static inline void ForEachPermutation(Container &container, Lambda &&lambda)
{
    Array<uint32> indices;

    for (uint32 i = 0; i < uint32(container.Size()); i++) {
        const uint32 num_combinations = (1u << i);

        for (uint32 k = 0; k < num_combinations; k++) {
            indices.Clear();

            for (uint32 j = 0; j < i; j++) {
                if ((k & (1u << j)) != (1u << j)) {
                    continue;
                }
                
                indices.PushBack(j);
            }
            
            indices.PushBack(i);

            lambda(indices);
        }
    }
}

/*! \brief Perform a parallel foreach with in the default pool (THREAD_POOL_GENERIC). */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        container,
        std::forward<Lambda>(lambda)
    );
}

/*! \brief Perform a parallel foreach within the given task thread pool \ref{pool}.
 *  Number of batches will depend upon the thread pool selected's number of workers. */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, TaskThreadPool &pool, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        pool,
        container,
        std::forward<Lambda>(lambda)
    );
}

/*! \brief Perform a parallel foreach within the given task thread pool \ref{pool} and using \ref{num_batches} batches. */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, uint32 num_batches, TaskThreadPool &pool, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        pool,
        num_batches,
        container,
        std::forward<Lambda>(lambda)
    );
}

} // namespace hyperion

#endif