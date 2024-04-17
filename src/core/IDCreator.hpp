/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_ID_CREATOR_HPP
#define HYPERION_CORE_ID_CREATOR_HPP

#include <core/containers/Queue.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/ID.hpp>
#include <Constants.hpp>
#include <Types.hpp>
#include <core/Defines.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

template <auto ... Args>
struct IDCreator
{
    TypeID              type_id;
    std::atomic<uint>   id_counter { 0u };
    std::mutex          free_id_mutex;
    Queue<uint>         free_indices;

    uint NextID()
    {
        std::lock_guard guard(free_id_mutex);

        if (free_indices.Empty()) {
            return id_counter.fetch_add(1) + 1;
        }

        return free_indices.Pop();
    }

    void FreeID(uint index)
    {
        std::lock_guard guard(free_id_mutex);

        free_indices.Push(index);
    }
};

} // namespace hyperion

#endif