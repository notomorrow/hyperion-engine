/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_ID_CREATOR_HPP
#define HYPERION_CORE_ID_CREATOR_HPP

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/Queue.hpp>
#include <core/containers/TypeMap.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

struct IDGenerator
{
    TypeID type_id;
    AtomicVar<uint32> id_counter;
    AtomicVar<uint32> num_free_indices;
    Queue<uint32> free_indices;
    Mutex free_id_mutex;

    IDGenerator(TypeID type_id = TypeID::Void())
        : type_id(type_id),
          id_counter(0),
          num_free_indices(0)
    {
    }

    IDGenerator(const IDGenerator&) = delete;
    IDGenerator& operator=(const IDGenerator&) = delete;

    IDGenerator(IDGenerator&& other) noexcept
        : type_id(std::move(other.type_id)),
          id_counter(other.id_counter.Exchange(0, MemoryOrder::ACQUIRE)),
          num_free_indices(other.num_free_indices.Exchange(0, MemoryOrder::ACQUIRE)),
          free_indices(std::move(other.free_indices))
    {
    }

    IDGenerator& operator=(IDGenerator&& other) noexcept = delete;

    ~IDGenerator() = default;

    uint32 NextID()
    {
        uint32 current_num_free_indices;

        if ((current_num_free_indices = num_free_indices.Get(MemoryOrder::ACQUIRE)) != 0)
        {
            Mutex::Guard guard(free_id_mutex);

            // Check that it hasn't changed before the lock
            if (free_indices.Any())
            {
                const uint32 index = free_indices.Pop();

                num_free_indices.Decrement(1, MemoryOrder::RELEASE);

                return index;
            }
        }

        return id_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
    }

    void FreeID(uint32 index)
    {
        AssertThrowMsg(index != 0, "Invalid index");

        Mutex::Guard guard(free_id_mutex);

        AssertDebug(!free_indices.Contains(index));

        free_indices.Push(index);
        num_free_indices.Increment(1, MemoryOrder::RELEASE);
    }

    void Reset()
    {
        Mutex::Guard guard(free_id_mutex);

        id_counter.Set(0, MemoryOrder::RELEASE);
        num_free_indices.Set(0, MemoryOrder::RELEASE);
        free_indices.Clear();
    }
};

} // namespace hyperion

#endif